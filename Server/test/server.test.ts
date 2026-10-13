import type { FastifyInstance } from 'fastify';
import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import WebSocket from 'ws';

import { defaultHubOptions } from '../src/hub.js';
import type { ServerMessage } from '../src/protocol.js';
import { buildServer } from '../src/server.js';
import { MemoryAccounts, MemoryBroker, MemoryCodes, MemoryLimiter, MemoryRegistry, MemoryStats } from './memory.js';

// 真的起两个服务端（两个"实例"），用真的 WebSocket 连上去
class Client {
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

    async open(): Promise<void> {
        if (this.socket.readyState === WebSocket.OPEN)
            return;
        await new Promise((resolve, reject) => {
            this.socket.once('open', resolve);
            this.socket.once('error', reject);
        });
    }

    send(message: object): void {
        this.socket.send(JSON.stringify(message));
    }

    next(): Promise<ServerMessage> {
        const ready = this.#received.shift();
        if (ready)
            return Promise.resolve(ready);
        return new Promise((resolve) => this.#waiters.push(resolve));
    }

    closed(): Promise<number> {
        return new Promise((resolve) => this.socket.once('close', (code) => resolve(code)));
    }
}

function hello(deviceId: string, token: string): object {
    const publicKey = Buffer.concat([Buffer.from([0x04]), Buffer.alloc(64, 7)]).toString('base64');
    return { type: 'hello', deviceId, token, publicKey, name: '电脑', platform: 'Windows', version: '0.1', machine: '' };
}

describe('server', () => {
    const apps: FastifyInstance[] = [];
    const clients: Client[] = [];
    let urls: string[];

    beforeEach(async () => {
        const registry = new MemoryRegistry();
        const accounts = new MemoryAccounts();
        const broker = new MemoryBroker();
        urls = [];
        for (let i = 0; i < 2; ++i) {
            const { app } = await buildServer({
                registry, accounts, bus: broker.bus(), limiter: new MemoryLimiter(), codes: new MemoryCodes(), mailer: null, stats: new MemoryStats(),
                logger: false,
            });
            await app.listen({ host: '127.0.0.1', port: 0 });
            const address = app.server.address();
            if (!address || typeof address === 'string')
                throw new Error('no port');
            urls.push(`ws://127.0.0.1:${address.port}/ws`);
            apps.push(app);
        }
    });

    afterEach(async () => {
        for (const client of clients.splice(0))
            client.socket.terminate();
        await Promise.all(apps.splice(0).map((app) => app.close()));
    });

    async function open(url: string): Promise<Client> {
        const client = new Client(url);
        clients.push(client);
        await client.open();
        return client;
    }

    it('answers /health', async () => {
        const response = await apps[0]!.inject({ method: 'GET', url: '/health' });
        expect(response.json()).toEqual({ ok: true, online: 0 });
    });

    it('logs in and relays signals between instances', async () => {
        const viewer = await open(urls[0]!);
        const sharer = await open(urls[1]!);
        viewer.send(hello('111111111', 'v'.repeat(43)));
        sharer.send(hello('222222222', 's'.repeat(43)));
        expect(await viewer.next()).toEqual({ type: 'welcome', ice: null });
        expect(await sharer.next()).toEqual({ type: 'welcome', ice: null });
        // 都没登录账号
        expect(await viewer.next()).toEqual({ type: 'account', account: null });
        expect(await sharer.next()).toEqual({ type: 'account', account: null });

        viewer.send({ type: 'signal', to: '222222222', session: 'one', text: 'offer', transport: 'p2p', access: 'password', sdk: '0.1.0-beta' });
        expect(await sharer.next())
            .toEqual({ type: 'signal', from: '111111111', name: '电脑', session: 'one', text: 'offer', transport: 'p2p', access: 'password', sdk: '0.1.0-beta' });
        sharer.send({ type: 'signal', to: '111111111', session: 'one', text: 'answer', transport: 'p2p', access: 'password', sdk: '0.1.0-beta' });
        expect(await viewer.next())
            .toEqual({ type: 'signal', from: '222222222', name: '电脑', session: 'one', text: 'answer', transport: 'p2p', access: 'password', sdk: '0.1.0-beta' });
    });

    // 中继的地址跟着设备连服务器用的主机名（Host 头，不带端口）
    it('gives the relay at the host the device connected to', async () => {
        const ice = { secret: 'dev-secret', host: null, port: 3478, transport: 'udp' as const, ttlS: 3600 };
        const { app } = await buildServer({
            registry: new MemoryRegistry(), accounts: new MemoryAccounts(), bus: new MemoryBroker().bus(),
            limiter: new MemoryLimiter(), codes: new MemoryCodes(), mailer: null, stats: new MemoryStats(), logger: false,
            hub: { ...defaultHubOptions, ice },
        });
        await app.listen({ host: '127.0.0.1', port: 0 });
        apps.push(app);
        const address = app.server.address();
        if (!address || typeof address === 'string')
            throw new Error('no port');
        const device = await open(`ws://localhost:${address.port}/ws`);
        device.send(hello('111111111', 'v'.repeat(43)));
        const welcome = await device.next();
        expect(welcome.type === 'welcome' && welcome.ice?.stun).toEqual(['localhost:3478']);
        expect(welcome.type === 'welcome' && welcome.ice?.turn.server).toBe('localhost:3478');
    });

    it('rejects binary messages', async () => {
        const client = await open(urls[0]!);
        const closed = client.closed();
        client.socket.send(Buffer.from([1, 2, 3]));
        expect(await client.next()).toEqual({ type: 'rejected', reason: 'invalid' });
        expect(await closed).toBe(4000);
    });
});
