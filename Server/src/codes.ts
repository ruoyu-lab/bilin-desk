import { createHash, randomInt } from 'node:crypto';

import { Redis } from 'ioredis';

import type { CodePurpose } from './protocol.js';

// 邮件里的验证码（注册、找回密码，docs/devices-and-accounts.md）：6 位数字，CODE_TTL_S 秒内有效；用对一次就作废，
// 错了 MAX_TRIES 次也作废；再发一个就换掉之前的。几个实例共用一份（Redis），只存哈希
export const CODE_TTL_S = 600;
export const MAX_TRIES = 5;

export interface Codes {
    // 给这个邮箱发的新验证码（之前的作废）
    issue(purpose: CodePurpose, email: string): Promise<string>;
    // 对不对：对了作废；错了记一次，错够 MAX_TRIES 次作废。过期了、没发过的都不对
    check(purpose: CodePurpose, email: string, code: string): Promise<boolean>;
    close(): Promise<void>;
}

export function newCode(): string {
    return String(randomInt(0, 1_000_000)).padStart(6, '0');
}

export function codeHash(purpose: CodePurpose, email: string, code: string): string {
    return createHash('sha256').update(`${purpose}\n${email}\n${code}`).digest('hex');
}

const PREFIX = 'bilin:code:';

// 核对要一步做完（几个实例同时试也不会多试几次）：对了删掉、回 1；错了记一次，到次数删掉；没有回 0
const CHECK = `
local stored = redis.call('HGET', KEYS[1], 'hash')
if not stored then return 0 end
if stored == ARGV[1] then
    redis.call('DEL', KEYS[1])
    return 1
end
if redis.call('HINCRBY', KEYS[1], 'tries', 1) >= tonumber(ARGV[2]) then redis.call('DEL', KEYS[1]) end
return 0`;

export class RedisCodes implements Codes {
    readonly #redis: Redis;

    constructor(url: string) {
        this.#redis = new Redis(url);
    }

    async issue(purpose: CodePurpose, email: string): Promise<string> {
        const code = newCode();
        const key = `${PREFIX}${purpose}:${email}`;
        await this.#redis.multi()
            .del(key)
            .hset(key, 'hash', codeHash(purpose, email, code), 'tries', 0)
            .expire(key, CODE_TTL_S)
            .exec();
        return code;
    }

    async check(purpose: CodePurpose, email: string, code: string): Promise<boolean> {
        const result = await this.#redis.eval(CHECK, 1, `${PREFIX}${purpose}:${email}`, codeHash(purpose, email, code), MAX_TRIES);
        return result === 1;
    }

    async close(): Promise<void> {
        await this.#redis.quit();
    }
}
