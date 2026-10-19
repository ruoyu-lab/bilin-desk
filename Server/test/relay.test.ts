import type { FastifyInstance } from 'fastify';
import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import WebSocket from 'ws';

import { buildRelay, MAX_VIEWERS, RelayClose } from '../src/relay/server.js';
import { MemoryTickets } from './memory.js';

const ROOM = '123456789';

// 一个连到转发进程的客户端：收到的二进制消息排起来
class Client {
    readonly socket: WebSocket;
    readonly #received: Buffer[] = [];
    readonly #waiters: Array<(data: Buffer) => void> = [];

    constructor(url: string) {
        this.socket = new WebSocket(url);
        this.socket.on('message', (data: Buffer) => {
            const waiter = this.#waiters.shift();
            if (waiter)
                waiter(data);
            else
                this.#received.push(data);
        });
    }

    async open(): Promise<void> {
        if (this.socket.readyState !== WebSocket.OPEN)
            await new Promise((resolve) => this.socket.once('open', resolve));
    }

    next(): Promise<Buffer> {
        const ready = this.#received.shift();
        return ready ? Promise.resolve(ready) : new Promise((resolve) => this.#waiters.push(resolve));
    }

    closed(): Promise<number> {
        if (this.socket.readyState === WebSocket.CLOSED)
            return Promise.resolve(-1);
        return new Promise((resolve) => this.socket.once('close', (code) => resolve(code)));
    }
}

function routed(route: bigint, text: string): Buffer {
    const header = Buffer.alloc(8);
    header.writeBigUInt64BE(route);
    return Buffer.concat([header, Buffer.from(text)]);
}

describe('网页版的转发进程', () => {
    let app: FastifyInstance;
    let tickets: MemoryTickets;
    let base: string;
    const clients: Client[] = [];

    beforeEach(async () => {
        tickets = new MemoryTickets();
        app = await buildRelay({ tickets, logger: false });
        await app.listen({ host: '127.0.0.1', port: 0 });
        const address = app.server.address();
        if (!address || typeof address === 'string')
            throw new Error('no port');
        base = `ws://127.0.0.1:${address.port}/relay`;
    });

    afterEach(async () => {
        for (const client of clients.splice(0))
            client.socket.terminate();
        await app.close();
    });

    async function join(role: 'share' | 'view', room = ROOM): Promise<Client> {
        const client = new Client(`${base}?ticket=${await tickets.issue({ room, role })}`);
        clients.push(client);
        await client.open();
        return client;
    }

    // 进了房间：转发进程核对通行证是异步的，先来回一趟，免得第一条消息赶在它前面
    async function settle(): Promise<void> {
        await new Promise((resolve) => setTimeout(resolve, 50));
    }

    it('浏览器发的带上路由号交给电脑，电脑按路由号交回对应的浏览器', async () => {
        const sharer = await join('share');
        const first = await join('view');
        const second = await join('view');
        await settle();

        first.socket.send(Buffer.from('握手一'));
        const fromFirst = await sharer.next();
        const firstRoute = fromFirst.readBigUInt64BE(0);
        expect(fromFirst.subarray(8).toString()).toBe('握手一');

        second.socket.send(Buffer.from('握手二'));
        const secondRoute = (await sharer.next()).readBigUInt64BE(0);
        expect(secondRoute).not.toBe(firstRoute);

        sharer.socket.send(routed(secondRoute, '给二'));
        sharer.socket.send(routed(firstRoute, '给一'));
        expect((await second.next()).toString()).toBe('给二');
        expect((await first.next()).toString()).toBe('给一');
    });

    it('浏览器走了告诉电脑（空的数据报），电脑不要它了就断开它', async () => {
        const sharer = await join('share');
        const viewer = await join('view');
        await settle();
        viewer.socket.send(Buffer.from('hi'));
        const route = (await sharer.next()).readBigUInt64BE(0);

        sharer.socket.send(routed(route, ''));
        expect(await viewer.closed()).toBe(RelayClose.Dropped);
        const left = await sharer.next();
        expect(left.length).toBe(8);
        expect(left.readBigUInt64BE(0)).toBe(route);
    });

    it('通行证只能用一次，不对的进不来', async () => {
        const ticket = await tickets.issue({ room: ROOM, role: 'view' });
        const first = new Client(`${base}?ticket=${ticket}`);
        const again = new Client(`${base}?ticket=${ticket}`);
        const wrong = new Client(`${base}?ticket=猜的`);
        clients.push(first, again, wrong);
        expect(await again.closed()).toBe(RelayClose.Ticket);
        expect(await wrong.closed()).toBe(RelayClose.Ticket);
        await first.open();
        expect(first.socket.readyState).toBe(WebSocket.OPEN);
    });

    it('电脑走了，房间里的浏览器都断开；不同房间互不相干', async () => {
        const sharer = await join('share');
        const viewer = await join('view');
        const elsewhere = await join('view', '987654321');
        await settle();
        sharer.socket.close();
        expect(await viewer.closed()).toBe(RelayClose.SharerGone);
        expect(elsewhere.socket.readyState).toBe(WebSocket.OPEN);
    });

    it('电脑从另一条连接进来：旧的断开', async () => {
        const old = await join('share');
        await join('share');
        expect(await old.closed()).toBe(RelayClose.Replaced);
    });

    it(`一个房间最多 ${MAX_VIEWERS} 个浏览器`, async () => {
        for (let i = 0; i < MAX_VIEWERS; i++)
            await join('view');
        await settle();
        const extra = await join('view');
        expect(await extra.closed()).toBe(RelayClose.Full);
    });

    it('电脑还没进来时浏览器发的丢掉', async () => {
        const viewer = await join('view');
        await settle();
        viewer.socket.send(Buffer.from('早了'));
        await settle();
        const sharer = await join('share');
        await settle();
        viewer.socket.send(Buffer.from('现在'));
        expect((await sharer.next()).subarray(8).toString()).toBe('现在');
    });
});
