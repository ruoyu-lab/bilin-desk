import { createReadStream } from 'node:fs';
import { readFile } from 'node:fs/promises';
import type { Readable } from 'node:stream';

import Fastify, { type FastifyInstance, type FastifyReply, type FastifyRequest } from 'fastify';

import { checkPassword } from '../accounts.js';
import { isFeedbackId, type FeedbackStore } from '../feedback.js';
import type { Limiter } from '../limiter.js';
import { EMAIL, MAX_EMAIL } from '../protocol.js';
import { PLATFORMS, ReleaseError, type Releases } from '../releases.js';
import type { Backups } from './backups.js';
import type { LogEntry } from './logs.js';
import type { Sessions } from './sessions.js';
import type { Status } from './status.js';

// 管理后台（docs/admin.md）：给软件的作者一个人用，看服务器在不在正常运行、备份数据库、发布新版本、看反馈。
// 在 Nginx 后面，接口在 base 下的 api/（正式服务器上 base 是空的：admin.bilindesk.com/api/；开发用的虚拟机只有
// 一个地址，是 /admin）；网页（Server/admin 编出来的静态文件）由 Nginx 在 base 下发。
// 密码只有一个，哈希存在 passwordFile 里（set-password.ts 写），登录时读：改了密码不用重启
export interface AdminOptions {
    base: string;
    passwordFile: string;
    sessions: Sessions;
    limiter: Limiter;
    backups: Backups;
    releases: Releases;
    // 用户提交的反馈（feedback.ts）
    feedback: FeedbackStore;
    status: () => Promise<Status>;
    // 最近的警告和出错、把日志清空（logs.ts）
    logs: () => Promise<LogEntry[]>;
    clearLogs: () => Promise<number>;
    // 给 to 发一封测试邮件；没发出去时抛出（message 是原因）
    testMail: (to: string) => Promise<void>;
    // 只经 HTTPS 发 cookie。开发用的虚拟机是 http，关掉
    secureCookie: boolean;
    logger: boolean;
}

const COOKIE = 'bilin_admin';
const SESSION_TTL_S = 12 * 3600;
// 登录试错：同一个地址 15 分钟最多 10 次；所有地址加起来 30 次（换着地址猜也猜不了几次）
const LOGIN_WINDOW_S = 15 * 60;
const LOGIN_PER_ADDRESS = 10;
const LOGIN_TOTAL = 30;
// 测试邮件也占 Resend 的额度：一小时最多发 10 封
const TEST_MAILS_PER_HOUR = 10;

function cookieValue(request: FastifyRequest, name: string): string | null {
    for (const part of (request.headers.cookie ?? '').split(';')) {
        const [key, ...value] = part.trim().split('=');
        if (key === name)
            return value.join('=');
    }
    return null;
}

async function readHash(file: string): Promise<string | null> {
    try {
        return (await readFile(file, 'utf8')).trim() || null;
    } catch {
        return null;
    }
}

export async function buildAdmin(options: AdminOptions): Promise<FastifyInstance> {
    const app = Fastify({ logger: options.logger, trustProxy: ['127.0.0.1', '::1'] });
    const { base, sessions } = options;
    const cookie = (token: string, maxAgeS: number) =>
        `${COOKIE}=${token}; Path=${base || '/'}; Max-Age=${maxAgeS}; HttpOnly; SameSite=Strict${options.secureCookie ? '; Secure' : ''}`;

    await app.register(async (admin) => {
        // 上传的安装包：不读进内存，原样交给 Releases 边收边存
        admin.addContentTypeParser('application/octet-stream', (_request, payload, done) => done(null, payload));

        // 改东西的请求只认这个网页自己发的：cookie 是 SameSite=Strict，再看一下 Origin
        admin.addHook('onRequest', async (request, reply) => {
            if (request.method === 'GET' || request.method === 'HEAD')
                return;
            const origin = request.headers.origin;
            if (origin && new URL(origin).host !== request.headers.host)
                return reply.code(403).send({ error: 'origin' });
        });

        const signedIn = async (request: FastifyRequest, reply: FastifyReply) => {
            const token = cookieValue(request, COOKIE);
            if (!token || !(await sessions.valid(token)))
                return reply.code(401).send({ error: 'signedOut' });
        };

        admin.post<{ Body: { password?: unknown } }>('/api/login', async (request, reply) => {
            const [mine, all] = await Promise.all([
                options.limiter.hit(`admin-login:${request.ip}`, LOGIN_WINDOW_S),
                options.limiter.hit('admin-login', LOGIN_WINDOW_S),
            ]);
            if (mine > LOGIN_PER_ADDRESS || all > LOGIN_TOTAL)
                return reply.code(429).send({ error: 'tooMany' });
            const hash = await readHash(options.passwordFile);
            if (!hash)
                return reply.code(503).send({ error: 'noPassword' });
            const password = request.body?.password;
            if (typeof password !== 'string' || !(await checkPassword(password, hash))) {
                request.log.warn('管理后台：密码不对');
                return reply.code(401).send({ error: 'wrong' });
            }
            const token = await sessions.create(SESSION_TTL_S);
            request.log.info('管理后台：登录了');
            return reply.header('set-cookie', cookie(token, SESSION_TTL_S)).send({ ok: true });
        });

        admin.post('/api/logout', async (request, reply) => {
            const token = cookieValue(request, COOKIE);
            if (token)
                await sessions.remove(token);
            return reply.header('set-cookie', cookie('', 0)).send({ ok: true });
        });

        admin.get('/api/status', { onRequest: signedIn }, async () => options.status());

        admin.get('/api/logs', { onRequest: signedIn }, async () => ({ entries: await options.logs() }));

        // 清空：日志文件真的清掉、删掉，找不回来
        admin.delete('/api/logs', { onRequest: signedIn }, async () => ({ cleared: await options.clearLogs() }));

        admin.post<{ Body: { to?: unknown } }>('/api/mail/test', { onRequest: signedIn }, async (request, reply) => {
            const to = typeof request.body?.to === 'string' ? request.body.to.trim() : '';
            if (to.length > MAX_EMAIL || !EMAIL.test(to))
                return reply.code(400).send({ error: 'email' });
            if (await options.limiter.hit('admin-test-mail', 3600) > TEST_MAILS_PER_HOUR)
                return reply.code(429).send({ error: 'tooMany' });
            try {
                await options.testMail(to);
            } catch (error) {
                request.log.error({ err: error }, '测试邮件没发出去');
                return reply.code(502).send({ error: 'mailFailed', message: error instanceof Error ? error.message : String(error) });
            }
            request.log.info('发了一封测试邮件');
            return { ok: true };
        });

        admin.get('/api/feedback', { onRequest: signedIn }, async () => ({
            feedback: await options.feedback.list(),
            notify: await options.feedback.notifyAddress(),
        }));

        // 有新的反馈时发邮件告诉谁：空的是不发
        admin.put<{ Body: { to?: unknown } }>('/api/feedback/notify', { onRequest: signedIn }, async (request, reply) => {
            const to = typeof request.body?.to === 'string' ? request.body.to.trim() : null;
            if (to === null || (to && (to.length > MAX_EMAIL || !EMAIL.test(to))))
                return reply.code(400).send({ error: 'email' });
            await options.feedback.setNotifyAddress(to || null);
            return { notify: to || null };
        });

        // 打开一条：记成看过了
        admin.post<{ Params: { id: string } }>('/api/feedback/:id/read', { onRequest: signedIn }, async (request, reply) => {
            const feedback = isFeedbackId(request.params.id) ? await options.feedback.markRead(request.params.id) : null;
            if (!feedback)
                return reply.code(404).send({ error: 'notFound' });
            return feedback;
        });

        // 日志：存的就是 gzip 压缩的文本，原样发（浏览器自己解开）
        admin.get<{ Params: { id: string } }>('/api/feedback/:id/logs', { onRequest: signedIn }, async (request, reply) => {
            const logs = isFeedbackId(request.params.id) ? await options.feedback.logs(request.params.id) : null;
            if (!logs)
                return reply.code(404).send({ error: 'notFound' });
            return reply
                .type('text/plain; charset=utf-8')
                .header('content-encoding', 'gzip')
                .header('content-disposition', `inline; filename="bilin-feedback-${request.params.id}.log"`)
                .send(logs);
        });

        admin.delete<{ Params: { id: string } }>('/api/feedback/:id', { onRequest: signedIn }, async (request, reply) => {
            if (!isFeedbackId(request.params.id) || !(await options.feedback.remove(request.params.id)))
                return reply.code(404).send({ error: 'notFound' });
            return reply.code(204).send();
        });

        admin.get('/api/backups', { onRequest: signedIn }, async () => ({
            backups: await options.backups.list(),
            running: options.backups.running,
        }));

        admin.post('/api/backups', { onRequest: signedIn }, async (request, reply) => {
            try {
                return await options.backups.create();
            } catch (error) {
                request.log.error({ err: error }, '备份没成');
                return reply.code(500).send({ error: 'backupFailed' });
            }
        });

        admin.get<{ Params: { name: string } }>('/api/backups/:name', { onRequest: signedIn }, async (request, reply) => {
            const file = await options.backups.file(request.params.name);
            if (!file)
                return reply.code(404).send({ error: 'notFound' });
            return reply
                .type('application/gzip')
                .header('content-disposition', `attachment; filename="${request.params.name}"`)
                .send(createReadStream(file));
        });

        admin.delete<{ Params: { name: string } }>('/api/backups/:name', { onRequest: signedIn }, async (request, reply) => {
            if (!(await options.backups.remove(request.params.name)))
                return reply.code(404).send({ error: 'notFound' });
            return reply.code(204).send();
        });

        admin.get('/api/releases', { onRequest: signedIn }, async () => ({
            platforms: await Promise.all(PLATFORMS.map(async (platform) => ({
                platform,
                current: (await options.releases.latest(platform))?.version ?? null,
                releases: await options.releases.list(platform),
            }))),
        }));

        // 发布：请求体是安装包，签名（scripts/sign-release.mjs 写的 .release.json 里的）在头里
        admin.put<{ Params: { platform: string; version: string } }>('/api/releases/:platform/:version',
            { onRequest: signedIn }, async (request, reply) => {
                const signature = request.headers['x-release-signature'];
                if (typeof signature !== 'string' || request.headers['content-type'] !== 'application/octet-stream')
                    return reply.code(400).send({ error: 'signature' });
                try {
                    const release = await options.releases.publish(request.params.platform, request.params.version,
                                                                   signature, request.body as Readable);
                    request.log.info({ platform: release.platform, version: release.version }, '发布了新版本');
                    return release;
                } catch (error) {
                    if (error instanceof ReleaseError)
                        return reply.code(400).send({ error: error.code });
                    throw error;
                }
            });

        admin.delete<{ Params: { platform: string; version: string } }>('/api/releases/:platform/:version',
            { onRequest: signedIn }, async (request, reply) => {
                const result = await options.releases.remove(request.params.platform, request.params.version);
                if (result === 'current')
                    return reply.code(409).send({ error: 'current' });
                if (result === 'notFound')
                    return reply.code(404).send({ error: 'notFound' });
                return reply.code(204).send();
            });
    }, { prefix: base });

    return app;
}
