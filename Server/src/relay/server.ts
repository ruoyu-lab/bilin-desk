import websocket from '@fastify/websocket';
import Fastify, { type FastifyInstance } from 'fastify';
import type { WebSocket } from 'ws';

import { REFRESH_MS, type RelayStatus } from '../relay-load.js';
import type { Tickets } from '../tickets.js';

// 网页版的转发进程（docs/web.md）：浏览器和被连的电脑都经 WebSocket 连到这里，按房间（被连的设备 ID）配对，
// 二进制消息原样转过去。转的是 SDK 的数据报（宿主自己的通道，RCS_TRANSPORT_HOST）：端到端加密、重传、码率、
// 重连都是两端的 SDK 做的，这里看不懂也不用懂。
// GET /relay?ticket=...：凭服务端发的一次性通行证（tickets.ts）进房间。
// - 观看端（浏览器）：一条消息一个数据报
// - 分享端（电脑）：一个房间一个，一条消息是 8 字节的路由号（大端）加一个数据报：路由号是这里给每个观看端的编号，
//   SDK 的 route。空的数据报：发来的是"这个观看端走了"，发出去的是"不要这个观看端了"
// 对端收不过来（积压超过 MAX_BUFFERED）时丢掉这个数据报，由 SDK 重传、降码率。
// 服务器的带宽是共用的（docs/web.md）：所有房间的浏览器加起来最多 maxViewers 个，满了的进不来；发给一个浏览器的超过
// viewerBytesPerSecond 的丢掉（SDK 当作丢包，降码率）。现在有几个、最多几个报给服务端（relay-load.ts），满了它直接说人太多
export interface RelayOptions {
    tickets: Tickets;
    status: RelayStatus;
    logger: boolean;
    maxViewers?: number;
    viewerBytesPerSecond?: number;
    heartbeatMs?: number;
}

export const RELAY_PATH = '/relay';
// SDK 的数据报最大 65535 字节，分享端的前面还有 8 字节路由号
const MAX_MESSAGE = 65535 + 8;
const MAX_BUFFERED = 1024 * 1024;
export const MAX_VIEWERS = 8;
const ROUTE_BYTES = 8;

// 关闭的原因（WebSocket 的关闭码）
export enum RelayClose {
    Ticket = 4401,     // 通行证不对、过期了、用过了
    Replaced = 4409,   // 这台电脑从另一条连接进了房间
    Full = 4429,       // 房间里的观看端满了
    SharerGone = 4410, // 被连的电脑走了
    Dropped = 4403,    // 被连的电脑不要这个观看端了
    Busy = 4503,       // 网页版的路数满了
}

// 发给一个浏览器的限速：令牌桶，最多攒一秒的量（关键帧一下子大）
class Bucket {
    #tokens: number;
    #last = performance.now();

    constructor(readonly perSecond: number) {
        this.#tokens = perSecond;
    }

    take(bytes: number): boolean {
        const now = performance.now();
        this.#tokens = Math.min(this.perSecond, this.#tokens + (now - this.#last) * this.perSecond / 1000);
        this.#last = now;
        if (this.#tokens < bytes)
            return false;
        this.#tokens -= bytes;
        return true;
    }
}

interface Viewer {
    socket: WebSocket;
    bucket: Bucket;
}

interface Room {
    sharer: WebSocket | null;
    viewers: Map<bigint, Viewer>;
}

function send(socket: WebSocket, data: Buffer): void {
    if (socket.readyState === socket.OPEN && socket.bufferedAmount <= MAX_BUFFERED)
        socket.send(data, { binary: true });
}

function routed(route: bigint, data: Buffer): Buffer {
    const header = Buffer.alloc(ROUTE_BYTES);
    header.writeBigUInt64BE(route);
    return Buffer.concat([header, data]);
}

export async function buildRelay(options: RelayOptions): Promise<FastifyInstance> {
    // 在本机的 Nginx 后面
    const app = Fastify({ logger: options.logger, trustProxy: ['127.0.0.1', '::1'] });
    // 内容是密文，压缩不了
    await app.register(websocket, { options: { maxPayload: MAX_MESSAGE, perMessageDeflate: false } });
    const rooms = new Map<string, Room>();
    let nextRoute = 1n;
    let viewers = 0;
    const heartbeatMs = options.heartbeatMs ?? 30_000;
    const maxViewers = options.maxViewers ?? 20;
    const viewerBytesPerSecond = options.viewerBytesPerSecond ?? 1_000_000;

    const report = () => {
        options.status.setLoad({ viewers, max: maxViewers }).catch((error: unknown) => app.log.error({ err: error }, '报不了网页版的路数'));
    };
    const reporter = setInterval(report, REFRESH_MS);
    app.addHook('onReady', async () => report());
    app.addHook('onClose', async () => clearInterval(reporter));

    const roomOf = (name: string): Room => {
        let room = rooms.get(name);
        if (!room) {
            room = { sharer: null, viewers: new Map() };
            rooms.set(name, room);
        }
        return room;
    };
    const forget = (name: string, room: Room) => {
        if (!room.sharer && room.viewers.size === 0)
            rooms.delete(name);
    };

    app.get<{ Querystring: { ticket?: string } }>(RELAY_PATH, { websocket: true }, async (socket, request) => {
        // 核对通行证的这一会儿到的消息收不到：都是 SDK 的数据报，它自己会再发
        const ticket = await options.tickets.redeem(request.query.ticket ?? '');
        if (!ticket) {
            socket.close(RelayClose.Ticket, 'ticket');
            return;
        }
        const room = roomOf(ticket.room);

        if (ticket.role === 'share') {
            room.sharer?.close(RelayClose.Replaced, 'replaced');
            room.sharer = socket;
            request.log.info({ room: ticket.room }, '转发：电脑进了房间');
            socket.on('message', (data: Buffer, isBinary) => {
                if (!isBinary || data.length < ROUTE_BYTES)
                    return;
                const route = data.readBigUInt64BE(0);
                const viewer = room.viewers.get(route);
                if (!viewer)
                    return;
                if (data.length === ROUTE_BYTES)
                    viewer.socket.close(RelayClose.Dropped, 'dropped');
                else if (viewer.bucket.take(data.length - ROUTE_BYTES))
                    send(viewer.socket, data.subarray(ROUTE_BYTES));
            });
            socket.on('close', () => {
                if (room.sharer !== socket)
                    return;
                room.sharer = null;
                // 观看端的 SDK 会重连：电脑回来以后它们拿新的通行证再进
                for (const viewer of room.viewers.values())
                    viewer.socket.close(RelayClose.SharerGone, 'sharer gone');
                forget(ticket.room, room);
                request.log.info({ room: ticket.room }, '转发：电脑走了');
            });
        } else {
            if (viewers >= maxViewers) {
                socket.close(RelayClose.Busy, 'busy');
                forget(ticket.room, room);
                return;
            }
            if (room.viewers.size >= MAX_VIEWERS) {
                socket.close(RelayClose.Full, 'full');
                forget(ticket.room, room);
                return;
            }
            const route = nextRoute++;
            room.viewers.set(route, { socket, bucket: new Bucket(viewerBytesPerSecond) });
            ++viewers;
            report();
            request.log.info({ room: ticket.room, route: String(route) }, '转发：浏览器进了房间');
            // 电脑还没进来时发的丢掉：SDK 的握手自己会再发
            socket.on('message', (data: Buffer, isBinary) => {
                if (isBinary && data.length > 0 && room.sharer)
                    send(room.sharer, routed(route, data));
            });
            socket.on('close', () => {
                room.viewers.delete(route);
                --viewers;
                report();
                if (room.sharer)
                    send(room.sharer, routed(route, Buffer.alloc(0)));
                forget(ticket.room, room);
                request.log.info({ room: ticket.room, route: String(route) }, '转发：浏览器走了');
            });
        }

        // 掉线的（睡眠了、网络断了）：上一次的 ping 没等到 pong 就断开
        let alive = true;
        socket.on('pong', () => {
            alive = true;
        });
        const heartbeat = setInterval(() => {
            if (!alive) {
                socket.terminate();
                return;
            }
            alive = false;
            socket.ping();
        }, heartbeatMs);
        socket.on('close', () => clearInterval(heartbeat));
    });

    app.get('/health', async () => ({ ok: true, rooms: rooms.size }));
    return app;
}
