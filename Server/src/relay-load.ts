import { Redis } from 'ioredis';

// 网页版现在有几路、最多几路（docs/web.md）：转发进程（relay/）隔 REFRESH_MS 报一次，服务端看满了没有，满了直接说人太多。
// LOAD_TTL_S 没报的算转发进程没在运行
export const LOAD_TTL_S = 60;
export const REFRESH_MS = 20_000;

export interface RelayLoad {
    viewers: number;
    max: number;
}

export interface RelayStatus {
    setLoad(load: RelayLoad): Promise<void>;
    // 转发进程没在运行（没报过）：null
    load(): Promise<RelayLoad | null>;
    close(): Promise<void>;
}

const KEY = 'bilin:relay:load';

export class RedisRelayStatus implements RelayStatus {
    readonly #redis: Redis;

    constructor(url: string) {
        this.#redis = new Redis(url);
    }

    async setLoad(load: RelayLoad): Promise<void> {
        await this.#redis.set(KEY, JSON.stringify(load), 'EX', LOAD_TTL_S);
    }

    async load(): Promise<RelayLoad | null> {
        const stored = await this.#redis.get(KEY);
        return stored ? JSON.parse(stored) as RelayLoad : null;
    }

    async close(): Promise<void> {
        await this.#redis.quit();
    }
}
