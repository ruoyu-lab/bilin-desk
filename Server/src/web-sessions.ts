import { randomBytes } from 'node:crypto';

import { Redis } from 'ioredis';

// 网页版登录着的账号（docs/web.md）：登录以后浏览器拿一个随机的令牌（HttpOnly 的 cookie），服务器记着它是哪个账号。
// 用着就往后延，WEB_SESSION_TTL_S 没用过就作废。几个实例共用一份（Redis）
export const WEB_SESSION_TTL_S = 30 * 24 * 60 * 60;
export const WEB_SESSION = /^[A-Za-z0-9_-]{43}$/;

export interface WebSessions {
    create(accountId: string): Promise<string>;
    // 令牌对应的账号（顺便延期）；没有、过期了：null
    account(token: string): Promise<string | null>;
    remove(token: string): Promise<void>;
    close(): Promise<void>;
}

export function newWebSession(): string {
    return randomBytes(32).toString('base64url');
}

const PREFIX = 'bilin:web-session:';

export class RedisWebSessions implements WebSessions {
    readonly #redis: Redis;

    constructor(url: string) {
        this.#redis = new Redis(url);
    }

    async create(accountId: string): Promise<string> {
        const token = newWebSession();
        await this.#redis.set(PREFIX + token, accountId, 'EX', WEB_SESSION_TTL_S);
        return token;
    }

    async account(token: string): Promise<string | null> {
        if (!WEB_SESSION.test(token))
            return null;
        return this.#redis.getex(PREFIX + token, 'EX', WEB_SESSION_TTL_S);
    }

    async remove(token: string): Promise<void> {
        await this.#redis.del(PREFIX + token);
    }

    async close(): Promise<void> {
        await this.#redis.quit();
    }
}
