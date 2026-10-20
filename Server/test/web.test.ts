import type { FastifyInstance, LightMyRequestResponse } from 'fastify';
import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import WebSocket from 'ws';

import type { ServerMessage } from '../src/protocol.js';
import { buildServer } from '../src/server.js';
import {
    MemoryAccounts, MemoryBroker, MemoryCodes, MemoryLimiter, MemoryMailer, MemoryRegistry, MemoryRelayStatus,
    MemoryStats, MemoryTickets, MemoryWakes, MemoryWebSessions,
} from './memory.js';

const DEVICE = '222222222';
const EMAIL = 'ahe@example.com';
const PASSWORD = 'correct horse';

// 被连的电脑：经长连接登录服务器，收服务器发来的消息
class Device {
    readonly socket: WebSocket;
    readonly #received: ServerMessage[] = [];
    readonly #waiters: Array<(message: ServerMessage) => void> = [];

    constructor(url: string) {
        this.socket = new WebSocket(url);
        this.socket.on('message', (data) => {
            const message = JSON.parse(data.toString()) as ServerMessage;
            const waiter = this.#waiters.shift();
            if (waiter)
                waiter(message);
            else
                this.#received.push(message);
        });
    }

    async login(): Promise<void> {
        await new Promise((resolve) => this.socket.once('open', resolve));
        const publicKey = Buffer.concat([Buffer.from([0x04]), Buffer.alloc(64, 7)]).toString('base64');
        this.socket.send(JSON.stringify({ type: 'hello', deviceId: DEVICE, token: 's'.repeat(43), publicKey, name: '书房的电脑',
                                          platform: 'Windows', version: '0.1.5-beta', machine: '' }));
        expect((await this.next()).type).toBe('welcome');
        expect((await this.next()).type).toBe('account');
    }

    // 下一条 type（别的跳过）
    async nextOf<T extends ServerMessage['type']>(type: T): Promise<Extract<ServerMessage, { type: T }>> {
        for (;;) {
            const message = await this.next();
            if (message.type === type)
                return message as Extract<ServerMessage, { type: T }>;
        }
    }

    next(): Promise<ServerMessage> {
        const ready = this.#received.shift();
        return ready ? Promise.resolve(ready) : new Promise((resolve) => this.#waiters.push(resolve));
    }

    answer(ticket: string, reason = ''): void {
        this.socket.send(JSON.stringify({ type: 'webReady', ticket, reason }));
    }
}

function cookieOf(response: LightMyRequestResponse): string {
    return String(response.headers['set-cookie']).split(';')[0]!;
}

describe('网页版的接口', () => {
    let app: FastifyInstance;
    let url: string;
    let registry: MemoryRegistry;
    let accounts: MemoryAccounts;
    let tickets: MemoryTickets;
    let relay: MemoryRelayStatus;
    let mailer: MemoryMailer;
    const devices: Device[] = [];

    beforeEach(async () => {
        registry = new MemoryRegistry();
        accounts = new MemoryAccounts();
        tickets = new MemoryTickets();
        relay = new MemoryRelayStatus();
        mailer = new MemoryMailer();
        ({ app } = await buildServer({
            registry, accounts, bus: new MemoryBroker().bus(), limiter: new MemoryLimiter(), codes: new MemoryCodes(), mailer,
            stats: new MemoryStats(), wakes: new MemoryWakes(), logger: false,
            web: { tickets, sessions: new MemoryWebSessions(), relay, answerWaitMs: 400, answerPollMs: 20 },
        }));
        await app.listen({ host: '127.0.0.1', port: 0 });
        const address = app.server.address();
        if (!address || typeof address === 'string')
            throw new Error('no port');
        url = `ws://127.0.0.1:${address.port}/ws`;
    });

    afterEach(async () => {
        for (const device of devices.splice(0))
            device.socket.terminate();
        await app.close();
    });

    async function online(): Promise<Device> {
        const device = new Device(url);
        devices.push(device);
        await device.login();
        return device;
    }

    const post = (path: string, payload: object, cookie = '') =>
        app.inject({ method: 'POST', url: `/web/${path}`, payload, headers: cookie ? { cookie } : {} });

    async function signedIn(): Promise<string> {
        await accounts.create(EMAIL, '阿禾', PASSWORD);
        const response = await post('login', { email: EMAIL, password: PASSWORD });
        expect(response.statusCode).toBe(200);
        return cookieOf(response);
    }

    it('登录、看账号、退出：cookie 只在 /web 下、HttpOnly、SameSite=Strict', async () => {
        expect((await app.inject({ url: '/web/me' })).json()).toEqual({ account: null, devices: [] });
        await accounts.create(EMAIL, '阿禾', PASSWORD);
        expect((await post('login', { email: EMAIL, password: '不对的密码' })).json()).toEqual({ error: 'wrongPassword' });

        const response = await post('login', { email: EMAIL.toUpperCase(), password: PASSWORD });
        const header = String(response.headers['set-cookie']);
        expect(header).toContain('Path=/web');
        expect(header).toContain('HttpOnly');
        expect(header).toContain('SameSite=Strict');
        expect(response.json()).toEqual({ account: { email: EMAIL, nickname: '阿禾' }, devices: [] });

        const cookie = cookieOf(response);
        expect((await app.inject({ url: '/web/me', headers: { cookie } })).json().account.email).toBe(EMAIL);
        await post('logout', {}, cookie);
        expect((await app.inject({ url: '/web/me', headers: { cookie } })).json().account).toBeNull();
    });

    it('账号下的设备带着在不在线', async () => {
        const cookie = await signedIn();
        await online();
        const account = await accounts.byEmail(EMAIL);
        await registry.bind(DEVICE, account!.id);
        expect((await app.inject({ url: '/web/me', headers: { cookie } })).json().devices)
            .toEqual([{ deviceId: DEVICE, name: '书房的电脑', platform: 'Windows', online: true, wake: 'none' }]);
    });

    it('注册：先发验证码，对了就登录', async () => {
        expect((await post('code', { email: EMAIL, purpose: 'register' })).json()).toEqual({ ok: true });
        expect((await post('register', { email: EMAIL, nickname: '阿禾', password: PASSWORD, code: '000000' })).statusCode).toBe(401);
        const registered = await post('register', { email: EMAIL, nickname: '阿禾', password: PASSWORD, code: mailer.codeFor(EMAIL) });
        expect(registered.json().account).toEqual({ email: EMAIL, nickname: '阿禾' });
        expect(String(registered.headers['set-cookie'])).toContain('bilin_web=');
    });

    it('找回密码：验证码对了设新密码、登录', async () => {
        await accounts.create(EMAIL, '阿禾', PASSWORD);
        expect((await post('code', { email: EMAIL, purpose: 'resetPassword' })).json()).toEqual({ ok: true });
        const reset = await post('reset', { email: EMAIL, password: '新的密码 12345', code: mailer.codeFor(EMAIL) });
        expect(reset.statusCode).toBe(200);
        expect((await post('login', { email: EMAIL, password: '新的密码 12345' })).statusCode).toBe(200);
    });

    it('别的网站发来的改动请求不认', async () => {
        const response = await app.inject({
            method: 'POST', url: '/web/login', payload: { email: EMAIL, password: PASSWORD },
            headers: { host: 'web.example.test', origin: 'https://evil.test' },
        });
        expect(response.statusCode).toBe(403);
    });

    it('连别人的电脑：电脑收到通知，准备好了浏览器才拿到通行证', async () => {
        const device = await online();
        const connecting = post('connect', { deviceId: DEVICE });
        const notice = await device.next();
        expect(notice.type).toBe('webViewer');
        const { ticket, key } = notice as { ticket: string; key?: string };
        expect(key).toBeUndefined();
        expect(await tickets.redeem(ticket)).toEqual({ room: DEVICE, role: 'share' });
        device.answer(ticket);
        const response = await connecting;
        expect(response.statusCode).toBe(200);
        expect(await tickets.redeem(response.json().ticket)).toEqual({ room: DEVICE, role: 'view' });
    });

    it('连我的设备：电脑和浏览器拿到同一把一次性钥匙，房间是 <ID>/mine', async () => {
        const cookie = await signedIn();
        const device = await online();
        await registry.bind(DEVICE, (await accounts.byEmail(EMAIL))!.id);
        const connecting = post('connect', { deviceId: DEVICE }, cookie);
        const notice = await device.next() as { ticket: string; key: string };
        expect(Buffer.from(notice.key, 'base64')).toHaveLength(32);
        expect(await tickets.redeem(notice.ticket)).toEqual({ room: `${DEVICE}/mine`, role: 'share' });
        device.answer(notice.ticket);
        const response = (await connecting).json<{ ticket: string; key: string }>();
        expect(response.key).toBe(notice.key);
        expect(await tickets.redeem(response.ticket)).toEqual({ room: `${DEVICE}/mine`, role: 'view' });
    });

    it('电脑说不行、一直不回答、不在线、转发进程没在运行、人太多：说清楚', async () => {
        const device = await online();
        const declined = post('connect', { deviceId: DEVICE });
        device.answer((await device.next() as { ticket: string }).ticket, 'transportBusy');
        expect((await declined).json()).toEqual({ error: 'declined', reason: 'transportBusy' });

        const silent = post('connect', { deviceId: DEVICE });
        await device.next();
        expect((await silent).json()).toEqual({ error: 'noAnswer' });

        expect((await post('connect', { deviceId: '333333333' })).json()).toEqual({ error: 'offline' });
        expect((await post('connect', { deviceId: '12345' })).statusCode).toBe(400);
        relay.current = { viewers: 20, max: 20 };
        expect((await post('connect', { deviceId: DEVICE })).json()).toEqual({ error: 'busy' });
        relay.current = null;
        expect((await post('connect', { deviceId: DEVICE })).json()).toEqual({ error: 'unavailable' });
    });

    it('叫醒我的设备：账号下开着的电脑帮忙发唤醒包；没登录、不是我的设备时不行', async () => {
        const cookie = await signedIn();
        const accountId = (await accounts.byEmail(EMAIL))!.id;
        const helper = await online();
        await registry.bind(DEVICE, accountId);
        const target = '333333333';
        await registry.claim({ id: target, tokenHash: 'h', name: '客厅的电脑', platform: 'macOS', version: '0.1.5-beta',
                               machine: '', publicKey: '' });
        await registry.bind(target, accountId);
        const nic = { mac: 'aa:bb:cc:dd:ee:01', address: '192.168.1.20', prefix: 24, link: 'ethernet', wake: 'enabled' } as const;
        await registry.setWake(target, [nic]);
        expect((await app.inject({ url: '/web/me', headers: { cookie } })).json().devices
            .find((d: { deviceId: string }) => d.deviceId === target)).toMatchObject({ online: false, wake: 'ready' });

        const waking = post('wake', { deviceId: target }, cookie);
        const request = await helper.nextOf('wakeRequest');
        // 网页版要叫醒的：by 是空的
        expect(request).toMatchObject({ deviceId: target, name: '客厅的电脑', by: '', interfaces: [nic] });
        helper.socket.send(JSON.stringify({ type: 'wakeAnswer', request: request.request, result: 'sent' }));
        expect((await waking).json()).toEqual({ helper: '书房的电脑' });

        expect((await post('wake', { deviceId: target })).json()).toEqual({ error: 'signedOut' });
        expect((await post('wake', { deviceId: '444444444' }, cookie)).json()).toEqual({ error: 'notMine' });
    });
});
