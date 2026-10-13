import { randomBytes } from 'node:crypto';

import { Redis } from 'ioredis';

// 管理后台登录以后的会话：浏览器的 cookie 里是一串随机数，这里记着它到什么时候有效。
// 存在 Redis 里：后台重启（部署）不用重新登录
export interface Sessions {
    // 新建一个，返回 cookie 里放的那串
    create(ttlS: number): Promise<string>;
    valid(token: string): Promise<boolean>;
    remove(token: string): Promise<void>;
    close(): Promise<void>;
}

const PREFIX = 'bilin:admin:session:';

export function newSessionToken(): string {
    return randomBytes(32).toString('base64url');
}

export class RedisSessions implements Sessions {
    readonly #redis: Redis;

    constructor(url: string) {
        this.#redis = new Redis(url);
    }

    async create(ttlS: number): Promise<string> {
        const token = newSessionToken();
        await this.#redis.set(PREFIX + token, '1', 'EX', ttlS);
        return token;
    }

    async valid(token: string): Promise<boolean> {
        return (await this.#redis.exists(PREFIX + token)) === 1;
    }

    async remove(token: string): Promise<void> {
        await this.#redis.del(PREFIX + token);
    }

    async close(): Promise<void> {
        await this.#redis.quit();
    }
}
