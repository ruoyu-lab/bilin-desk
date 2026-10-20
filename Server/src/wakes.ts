import { Redis } from 'ioredis';

import type { WakeAnswerResult } from './protocol.js';

// 远程唤醒（protocol.ts 的 wakeRequest）：帮忙叫醒的电脑的回答。它们可能连在别的实例上，回答记进 Redis，
// 要叫醒的那一方的实例来看。WAKE_TTL_S 秒后删掉
export const WAKE_TTL_S = 60;

export interface Wakes {
    // 电脑 helper 对这次唤醒的回答
    answer(request: string, helper: string, result: WakeAnswerResult): Promise<void>;
    // 到现在为止的回答：电脑 → 回答
    answers(request: string): Promise<Map<string, WakeAnswerResult>>;
    close(): Promise<void>;
}

const PREFIX = 'bilin:wake:';

export class RedisWakes implements Wakes {
    readonly #redis: Redis;

    constructor(url: string) {
        this.#redis = new Redis(url);
    }

    async answer(request: string, helper: string, result: WakeAnswerResult): Promise<void> {
        await this.#redis.multi().hset(PREFIX + request, helper, result).expire(PREFIX + request, WAKE_TTL_S).exec();
    }

    async answers(request: string): Promise<Map<string, WakeAnswerResult>> {
        return new Map(Object.entries(await this.#redis.hgetall(PREFIX + request)) as Array<[string, WakeAnswerResult]>);
    }

    async close(): Promise<void> {
        await this.#redis.quit();
    }
}
