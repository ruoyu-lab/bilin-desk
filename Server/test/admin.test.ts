import { mkdtemp, readdir, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';

import type { FastifyInstance } from 'fastify';
import { afterEach, beforeEach, describe, expect, it } from 'vitest';

import { generateKeyPairSync } from 'node:crypto';
import { Readable } from 'node:stream';

import { hashPassword } from '../src/accounts.js';
import { Backups, isBackupName } from '../src/admin/backups.js';
import { clearLogs, recentProblems, type LogEntry } from '../src/admin/logs.js';
import { buildAdmin } from '../src/admin/server.js';
import { newSessionToken, type Sessions } from '../src/admin/sessions.js';
import type { Status } from '../src/admin/status.js';
import { Releases } from '../src/releases.js';
import { signRelease } from './releases.test.js';
import { MemoryLimiter } from './memory.js';

const BASE = '/admin';
const PASSWORD = 'correct horse battery';

class MemorySessions implements Sessions {
    readonly tokens = new Set<string>();

    async create(): Promise<string> {
        const token = newSessionToken();
        this.tokens.add(token);
        return token;
    }

    async valid(token: string): Promise<boolean> {
        return this.tokens.has(token);
    }

    async remove(token: string): Promise<void> {
        this.tokens.delete(token);
    }

    async close(): Promise<void> {}
}

const status: Status = {
    uptimeS: 3600,
    memory: { totalBytes: 1000, availableBytes: 400 },
    disk: { totalBytes: 2000, availableBytes: 1500 },
    databaseBytes: 123,
    devices: { registered: 5, online: 2 },
    versions: [{ platform: 'macOS', version: '0.1', devices: 5 }],
    accounts: 3,
    services: { server: true, mongod: true },
    system: { os: 'Rocky Linux 9', kernel: '5.14', cpus: 2, load: [0.1, 0.2, 0.3], node: 'v24', mongodb: '8.0', redis: '7.2' },
    certificate: { host: 'www.example.com', validTo: '2027-01-11T04:50:09.000Z' },
    backup: null,
    mail: { mode: 'resend', today: 3, limit: 90, lastFailure: null },
    trends: [{ day: '2026-10-13', devices: 1, accounts: 1, connections: 4, peakOnline: 2 }],
};

const problems: LogEntry[] = [
    { time: '2026-10-13T06:00:00.000Z', level: 'error', app: 'bilin-server', message: '验证码邮件没发出去', detail: 'Resend 回了 403' },
];

const releaseKeys = generateKeyPairSync('ec', { namedCurve: 'P-256' });

describe('管理后台', () => {
    let directory: string;
    let app: FastifyInstance;
    let dumps: number;
    let failDump: boolean;
    // 测试邮件发给了谁；mailError 不为空时发不出去
    let testMails: string[];
    let logsCleared: number;
    let mailError: string | null;

    beforeEach(async () => {
        directory = await mkdtemp(path.join(tmpdir(), 'bilin-admin-'));
        await writeFile(path.join(directory, 'password'), await hashPassword(PASSWORD));
        dumps = 0;
        testMails = [];
        logsCleared = 0;
        mailError = null;
        failDump = false;
        // 假的 mongodump：写一个小文件；名字里的时间到秒，连着备份时等一秒
        const backups = new Backups(path.join(directory, 'backups'), 2, async (file) => {
            if (failDump)
                throw new Error('mongodump 出错');
            dumps++;
            await new Promise((resolve) => setTimeout(resolve, 1001));
            await writeFile(file, `backup ${dumps}`);
        });
        app = await buildAdmin({
            base: BASE,
            passwordFile: path.join(directory, 'password'),
            sessions: new MemorySessions(),
            limiter: new MemoryLimiter(),
            backups,
            releases: new Releases(path.join(directory, 'updates'), releaseKeys.publicKey),
            status: async () => status,
            logs: async () => problems,
            clearLogs: async () => {
                logsCleared++;
                return 3;
            },
            testMail: async (to) => {
                if (mailError)
                    throw new Error(mailError);
                testMails.push(to);
            },
            secureCookie: true,
            logger: false,
        });
    });

    afterEach(async () => {
        await app.close();
        await rm(directory, { recursive: true, force: true });
    });

    async function login(password = PASSWORD) {
        return app.inject({ method: 'POST', url: `${BASE}/api/login`, payload: { password } });
    }

    async function signedIn(): Promise<string> {
        const response = await login();
        expect(response.statusCode).toBe(200);
        return String(response.headers['set-cookie']).split(';')[0]!;
    }

    it('只有 base 下的接口，网页由 Nginx 发', async () => {
        expect((await app.inject({ url: `${BASE}/` })).statusCode).toBe(404);
        expect((await app.inject({ url: '/api/status' })).statusCode).toBe(404);
    });

    it('没登录时接口都不给', async () => {
        for (const [method, url] of [['GET', 'status'], ['GET', 'backups'], ['POST', 'backups'],
                                     ['GET', 'backups/bilin-20261011-040000.archive.gz']] as const) {
            const response = await app.inject({ method, url: `${BASE}/api/${url}` });
            expect(response.statusCode).toBe(401);
        }
        expect((await app.inject({ url: `${BASE}/api/status`, headers: { cookie: 'bilin_admin=猜的' } })).statusCode).toBe(401);
    });

    it('密码对了给 cookie：只在这个地址下、只经 HTTPS、脚本读不到', async () => {
        expect((await login('不对')).statusCode).toBe(401);
        const response = await login();
        const cookie = String(response.headers['set-cookie']);
        expect(cookie).toContain(`Path=${BASE}`);
        expect(cookie).toContain('HttpOnly');
        expect(cookie).toContain('Secure');
        expect(cookie).toContain('SameSite=Strict');
        const status = await app.inject({ url: `${BASE}/api/status`, headers: { cookie: cookie.split(';')[0]! } });
        expect(status.json()).toMatchObject({ accounts: 3, devices: { online: 2 } });
    });

    it('退出以后 cookie 不再有用', async () => {
        const cookie = await signedIn();
        await app.inject({ method: 'POST', url: `${BASE}/api/logout`, headers: { cookie } });
        expect((await app.inject({ url: `${BASE}/api/status`, headers: { cookie } })).statusCode).toBe(401);
    });

    it('试错太多次：同一个地址 10 次以后连对的密码也不让试', async () => {
        for (let i = 0; i < 10; i++)
            expect((await login('不对')).statusCode).toBe(401);
        expect((await login()).statusCode).toBe(429);
    });

    it('还没设密码时说清楚', async () => {
        await rm(path.join(directory, 'password'));
        expect((await login()).json()).toEqual({ error: 'noPassword' });
    });

    it('别的网站发来的改动请求不认', async () => {
        const cookie = await signedIn();
        const response = await app.inject({
            method: 'POST',
            url: `${BASE}/api/backups`,
            headers: { cookie, host: 'example.test', origin: 'https://evil.test' },
        });
        expect(response.statusCode).toBe(403);
        expect(dumps).toBe(0);
    });

    it('备份：立即备份、列出来、下载、删除，只留最新的几份', async () => {
        const cookie = await signedIn();
        for (let i = 0; i < 3; i++)
            expect((await app.inject({ method: 'POST', url: `${BASE}/api/backups`, headers: { cookie } })).statusCode).toBe(200);
        const { backups } = (await app.inject({ url: `${BASE}/api/backups`, headers: { cookie } })).json<{
            backups: Array<{ name: string; size: number }>;
        }>();
        expect(backups).toHaveLength(2);
        expect(backups.every((b) => isBackupName(b.name))).toBe(true);

        const newest = backups[0]!.name;
        const download = await app.inject({ url: `${BASE}/api/backups/${newest}`, headers: { cookie } });
        expect(download.statusCode).toBe(200);
        expect(download.body).toBe('backup 3');
        expect(download.headers['content-disposition']).toContain(newest);

        expect((await app.inject({ method: 'DELETE', url: `${BASE}/api/backups/${newest}`, headers: { cookie } })).statusCode).toBe(204);
        expect(await readdir(path.join(directory, 'backups'))).toHaveLength(1);
    });

    it('备份：名字不对的不给（不能借它读别的文件）', async () => {
        const cookie = await signedIn();
        for (const name of ['..%2Fpassword', 'password', 'bilin-x.archive.gz'])
            expect((await app.inject({ url: `${BASE}/api/backups/${name}`, headers: { cookie } })).statusCode).toBe(404);
    });

    it('发布：签名对的安装包收下，列出来；签名不对的、不比现在新的不收；现在的那个不能删', async () => {
        const cookie = await signedIn();
        const upload = (version: string, bytes: Buffer, signature: string) => app.inject({
            method: 'PUT',
            url: `${BASE}/api/releases/windows-x64/${version}`,
            headers: { cookie, 'content-type': 'application/octet-stream', 'x-release-signature': signature },
            payload: Readable.from([bytes]),
        });
        const first = Buffer.from('安装包 0.2');
        expect((await upload('0.2', first, signRelease(releaseKeys.privateKey, 'windows-x64', '0.2', first))).statusCode).toBe(200);
        // 签的是另一个文件
        expect((await upload('0.3', Buffer.from('别的'), signRelease(releaseKeys.privateKey, 'windows-x64', '0.3', first))).json())
            .toEqual({ error: 'signature' });
        expect((await upload('0.1', first, signRelease(releaseKeys.privateKey, 'windows-x64', '0.1', first))).json())
            .toEqual({ error: 'notNewer' });
        const second = Buffer.from('安装包 0.3');
        expect((await upload('0.3', second, signRelease(releaseKeys.privateKey, 'windows-x64', '0.3', second))).statusCode).toBe(200);

        const { platforms } = (await app.inject({ url: `${BASE}/api/releases`, headers: { cookie } })).json<{
            platforms: Array<{ platform: string; current: string | null; releases: Array<{ version: string }> }>;
        }>();
        const windows = platforms.find((p) => p.platform === 'windows-x64')!;
        expect(windows.current).toBe('0.3');
        expect(windows.releases.map((r) => r.version)).toEqual(['0.3', '0.2']);

        expect((await app.inject({ method: 'DELETE', url: `${BASE}/api/releases/windows-x64/0.3`, headers: { cookie } })).statusCode).toBe(409);
        expect((await app.inject({ method: 'DELETE', url: `${BASE}/api/releases/windows-x64/0.2`, headers: { cookie } })).statusCode).toBe(204);
    });

    it('备份没成：不留半截文件', async () => {
        const cookie = await signedIn();
        failDump = true;
        expect((await app.inject({ method: 'POST', url: `${BASE}/api/backups`, headers: { cookie } })).statusCode).toBe(500);
        expect(await readdir(path.join(directory, 'backups'))).toEqual([]);
    });
    it('登录以后看得到最近的警告和出错', async () => {
        expect((await app.inject({ url: `${BASE}/api/logs` })).statusCode).toBe(401);
        const cookie = await signedIn();
        const response = await app.inject({ url: `${BASE}/api/logs`, headers: { cookie } });
        expect(response.json()).toEqual({ entries: problems });
    });

    it('日志里只挑警告和出错，新的在前', async () => {
        const line = (level: number, time: number, msg: string, err?: string) =>
            JSON.stringify({ level, time, pid: 1, msg, ...(err ? { err: { message: err } } : {}) });
        await writeFile(path.join(directory, 'bilin-server-out-0.log'),
                        [line(30, 1, '设备上线'), line(40, 2, '发起的连接太多'), line(50, 3, '处理消息时出错', 'boom')].join('\n'));
        await writeFile(path.join(directory, 'bilin-admin-error.log'), 'TypeError: x is not a function\n');
        await writeFile(path.join(directory, 'other.txt'), line(50, 9, '不是日志'));
        const entries = await recentProblems(directory);
        expect(entries.map((e) => [e.app, e.level, e.message, e.detail])).toEqual([
            ['bilin-server', 'error', '处理消息时出错', 'boom'],
            ['bilin-server', 'warn', '发起的连接太多', ''],
            ['bilin-admin', 'error', 'TypeError: x is not a function', ''],
        ]);
    });
    it('发测试邮件：要登录、邮箱要对，没发出去时说原因', async () => {
        const send = (to: unknown, cookie?: string) => app.inject({
            method: 'POST', url: `${BASE}/api/mail/test`, payload: { to }, headers: cookie ? { cookie } : {},
        });
        expect((await send('li@example.com')).statusCode).toBe(401);
        const cookie = await signedIn();
        expect((await send('不是邮箱', cookie)).statusCode).toBe(400);
        expect((await send(' li@example.com ', cookie)).json()).toEqual({ ok: true });
        expect(testMails).toEqual(['li@example.com']);
        mailError = 'Resend 回了 403：domain is not verified';
        const failed = await send('li@example.com', cookie);
        expect(failed.statusCode).toBe(502);
        expect(failed.json()).toEqual({ error: 'mailFailed', message: 'Resend 回了 403：domain is not verified' });
    });
    it('登录以后才能清空日志', async () => {
        expect((await app.inject({ method: 'DELETE', url: `${BASE}/api/logs` })).statusCode).toBe(401);
        expect(logsCleared).toBe(0);
        const cookie = await signedIn();
        expect((await app.inject({ method: 'DELETE', url: `${BASE}/api/logs`, headers: { cookie } })).json()).toEqual({ cleared: 3 });
        expect(logsCleared).toBe(1);
    });

    it('清空日志：正在写的清成空的，轮换下来的删掉，别的文件不动', async () => {
        await writeFile(path.join(directory, 'bilin-server-out-0.log'), '{"level":50,"msg":"x"}\n');
        await writeFile(path.join(directory, 'bilin-admin-error.log'), 'TypeError\n');
        await writeFile(path.join(directory, 'bilin-server-out-0.log-20261012.gz'), 'old');
        await writeFile(path.join(directory, 'bilin-server-error-0.log.1'), 'old');
        await writeFile(path.join(directory, 'other.txt'), 'keep');
        expect(await clearLogs(directory)).toBe(4);
        const left = await readdir(directory);
        expect(left).toContain('bilin-server-out-0.log');
        expect(left).not.toContain('bilin-server-out-0.log-20261012.gz');
        expect(left).not.toContain('bilin-server-error-0.log.1');
        expect(left).toContain('other.txt');
        expect(await recentProblems(directory)).toEqual([]);
    });
});
