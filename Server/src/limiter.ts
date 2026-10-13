import { Redis } from 'ioredis';

// 限速用的计数：每个键在一个时间窗里数了几次（窗从第一次数开始，到点清零）。几个实例共用一份（Redis），
// 换一个实例、断线重连都不会从零数起
export interface Limiter {
    // 数一次，返回这个窗里一共几次
    hit(key: string, windowS: number): Promise<number>;
    // 这个窗里已经几次（不数）
    count(key: string): Promise<number>;
    close(): Promise<void>;
}

const PREFIX = 'bilin:limit:';

export class RedisLimiter implements Limiter {
    readonly #redis: Redis;

    constructor(url: string) {
        this.#redis = new Redis(url);
    }

    async hit(key: string, windowS: number): Promise<number> {
        const [[, count]] = (await this.#redis.multi().incr(PREFIX + key).expire(PREFIX + key, windowS, 'NX').exec()) as [
            [Error | null, number],
            [Error | null, number],
        ];
        return count;
    }

    async count(key: string): Promise<number> {
        return Number((await this.#redis.get(PREFIX + key)) ?? 0);
    }

    async close(): Promise<void> {
        await this.#redis.quit();
    }
}
