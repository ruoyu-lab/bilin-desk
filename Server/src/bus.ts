import { Redis } from 'ioredis';

import type { Access, Transport } from './protocol.js';

// 实例之间的消息。每个实例订阅连在自己身上的设备，发给某台设备就是发布到它的频道；
// publish 返回收到的实例数，0 就是这台设备现在不在线
export type BusMessage =
    | {
        type: 'signal'; from: string; name: string; session: string; text: string; transport: Transport; access: Access;
        sdk: string;
    }
    | { type: 'decline'; from: string; session: string; reason: string }
    // 有浏览器要连这台设备（ServerMessage 的 webViewer）
    | { type: 'webViewer'; ticket: string; key?: string }
    // 这台设备在 connection 这条连接上重新登录了：它别的连接都关掉
    | { type: 'replaced'; connection: string }
    // 这台设备的账号变了（登录、退出、被移出），或者账号下的设备有变化（上下线、改名）：重新告诉它
    | { type: 'accountChanged' };

// 设备在不在线：连着的实例隔一会儿报一次（PRESENCE_TTL_S 没报的算不在线：实例崩了也不会一直"在线"）
export const PRESENCE_TTL_S = 90;

export interface Bus {
    publish(deviceId: string, message: BusMessage): Promise<number>;
    subscribe(deviceId: string): Promise<void>;
    unsubscribe(deviceId: string): Promise<void>;
    onMessage(handler: (deviceId: string, message: BusMessage) => void): void;
    // 这些设备连在本实例上
    markOnline(deviceIds: string[]): Promise<void>;
    markOffline(deviceId: string): Promise<void>;
    // 里面哪些在线
    online(deviceIds: string[]): Promise<Set<string>>;
    close(): Promise<void>;
}

const CHANNEL = 'bilin:device:';
const PRESENCE = 'bilin:presence:';

// Redis 的发布 / 订阅：发布和订阅各用一条连接（订阅中的连接不能发别的命令）。断线重连后 ioredis 会重新订阅
export class RedisBus implements Bus {
    readonly #publisher: Redis;
    readonly #subscriber: Redis;
    #handler: (deviceId: string, message: BusMessage) => void = () => {};

    constructor(url: string) {
        this.#publisher = new Redis(url);
        this.#subscriber = new Redis(url);
        this.#subscriber.on('message', (channel: string, payload: string) => {
            if (channel.startsWith(CHANNEL))
                this.#handler(channel.slice(CHANNEL.length), JSON.parse(payload) as BusMessage);
        });
    }

    publish(deviceId: string, message: BusMessage): Promise<number> {
        return this.#publisher.publish(CHANNEL + deviceId, JSON.stringify(message));
    }

    async subscribe(deviceId: string): Promise<void> {
        await this.#subscriber.subscribe(CHANNEL + deviceId);
    }

    async unsubscribe(deviceId: string): Promise<void> {
        await this.#subscriber.unsubscribe(CHANNEL + deviceId);
    }

    onMessage(handler: (deviceId: string, message: BusMessage) => void): void {
        this.#handler = handler;
    }

    async markOnline(deviceIds: string[]): Promise<void> {
        if (deviceIds.length === 0)
            return;
        const pipeline = this.#publisher.pipeline();
        for (const id of deviceIds)
            pipeline.set(PRESENCE + id, '1', 'EX', PRESENCE_TTL_S);
        await pipeline.exec();
    }

    async markOffline(deviceId: string): Promise<void> {
        await this.#publisher.del(PRESENCE + deviceId);
    }

    async online(deviceIds: string[]): Promise<Set<string>> {
        if (deviceIds.length === 0)
            return new Set();
        const values = await this.#publisher.mget(deviceIds.map((id) => PRESENCE + id));
        return new Set(deviceIds.filter((_, i) => values[i] !== null));
    }

    async close(): Promise<void> {
        await Promise.all([this.#publisher.quit(), this.#subscriber.quit()]);
    }
}
