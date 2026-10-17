import type { FastifyInstance } from 'fastify';
import { afterEach, beforeEach, describe, expect, it } from 'vitest';

import { decompressLogs, MAX_FEEDBACK_TEXT, MAX_LOGS_BYTES, parseFeedback } from '../src/feedback.js';
import { hashToken } from '../src/registry.js';
import { buildServer } from '../src/server.js';
import {
    MemoryAccounts, MemoryBroker, MemoryCodes, MemoryFeedback, MemoryLimiter, MemoryMailer, MemoryRegistry, MemoryStats,
} from './memory.js';

const DEVICE = '123456789';
const TOKEN = 'a'.repeat(43);
const ADMIN_URL = 'https://admin.test/admin/#feedback';

function request(overrides: object = {}): object {
    return {
        deviceId: DEVICE, token: TOKEN, version: '0.1.5-beta', system: 'macOS 15.3', contact: '',
        text: '连上以后画面是黑的', logs: [{ name: 'service.log', text: '第一行\n第二行\n' }], ...overrides,
    };
}

describe('反馈', () => {
    let app: FastifyInstance;
    let registry: MemoryRegistry;
    let accounts: MemoryAccounts;
    let feedback: MemoryFeedback;
    let mailer: MemoryMailer;
    let stats: MemoryStats;

    beforeEach(async () => {
        registry = new MemoryRegistry();
        accounts = new MemoryAccounts();
        feedback = new MemoryFeedback();
        mailer = new MemoryMailer();
        stats = new MemoryStats();
        await registry.claim({
            id: DEVICE, tokenHash: hashToken(TOKEN), name: '书房的电脑', platform: 'macOS', version: '0.1.5-beta',
            machine: '', publicKey: '',
        });
        ({ app } = await buildServer({
            registry, accounts, bus: new MemoryBroker().bus(), limiter: new MemoryLimiter(), codes: new MemoryCodes(),
            mailer, stats, logger: false, feedback: { store: feedback, adminUrl: ADMIN_URL },
        }));
    });

    afterEach(async () => {
        await app.close();
    });

    const submit = (body: object) => app.inject({ method: 'POST', url: '/feedback', payload: body });

    it('设备 ID 和令牌对得上就收：说的话、日志、版本和系统，设备的名字', async () => {
        const response = await submit(request({ contact: '  微信 abc  ' }));
        expect(response.statusCode).toBe(200);
        const [item] = feedback.items;
        expect(item).toMatchObject({
            deviceId: DEVICE, deviceName: '书房的电脑', platform: 'macOS', version: '0.1.5-beta', system: 'macOS 15.3',
            contact: '微信 abc', signedIn: false, text: '连上以后画面是黑的', read: false,
        });
        expect(await decompressLogs(item!.logs!)).toBe('===== service.log =====\n第一行\n第二行\n');
    });

    it('令牌不对、没登记的设备不收', async () => {
        expect((await submit(request({ token: 'b'.repeat(43) }))).statusCode).toBe(401);
        expect((await submit(request({ deviceId: '987654321' }))).statusCode).toBe(401);
        expect(feedback.items).toHaveLength(0);
    });

    it('登录了的：联系方式是账号的邮箱', async () => {
        const account = await accounts.create('me@example.com', '小明', 'password123');
        if (account === 'emailTaken')
            throw new Error('emailTaken');
        await registry.bind(DEVICE, account.id);
        await submit(request({ contact: '别的' }));
        expect(feedback.items[0]).toMatchObject({ contact: 'me@example.com', signedIn: true });
    });

    it('不带日志也行', async () => {
        expect((await submit(request({ logs: [] }))).statusCode).toBe(200);
        expect(feedback.items[0]).toMatchObject({ logsBytes: 0, logs: null });
    });

    it('说的话空的、太长，日志太大：不收', async () => {
        for (const body of [
            request({ text: '   ' }),
            request({ text: 'x'.repeat(MAX_FEEDBACK_TEXT + 1) }),
            request({ logs: [{ name: 'service.log', text: 'x'.repeat(MAX_LOGS_BYTES + 1) }] }),
            request({ logs: 'service.log' }),
            request({ contact: undefined }),
        ])
            expect((await submit(body)).statusCode).toBe(400);
        expect(feedback.items).toHaveLength(0);
    });

    it('同一台设备一天最多 5 条', async () => {
        for (let i = 0; i < 5; i++)
            expect((await submit(request())).statusCode).toBe(200);
        expect((await submit(request())).statusCode).toBe(429);
        expect(feedback.items).toHaveLength(5);
    });

    it('后台设了邮箱就发通知；没设不发', async () => {
        await submit(request());
        expect(mailer.mails).toHaveLength(0);
        await feedback.setNotifyAddress('author@example.com');
        await submit(request({ text: '<b>画面</b>卡' }));
        expect(mailer.mails).toHaveLength(1);
        const [mail] = mailer.mails;
        expect(mail!.to).toBe('author@example.com');
        expect(mail!.subject).toContain('<b>画面</b>卡');
        expect(mail!.html).toContain('&lt;b&gt;画面&lt;/b&gt;卡');
        expect(mail!.html).toContain(ADMIN_URL);
        expect(stats.counts.get('mails')).toBe(1);
    });

    it('通知邮件没发出去：反馈照样收下，记一次发信失败', async () => {
        await feedback.setNotifyAddress('author@example.com');
        mailer.failing = true;
        expect((await submit(request())).statusCode).toBe(200);
        expect(feedback.items).toHaveLength(1);
        expect(stats.lastMailFailure).toBe('发不出去');
    });

    it('parseFeedback 去掉说的话、联系方式两头的空白', () => {
        expect(parseFeedback(request({ text: '  卡  ', contact: ' qq ' }))).toMatchObject({ text: '卡', contact: 'qq' });
        expect(parseFeedback(null)).toBeNull();
    });
});
