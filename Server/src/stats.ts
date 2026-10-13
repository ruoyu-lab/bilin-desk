import { Redis } from 'ioredis';

// 每天的数目（管理后台的"最近 7 天"）：服务端的各个实例、管理后台往 Redis 里记，按服务器的本地日期，留 40 天。
// 只有数目：发起了几次连接、发出了几封验证码邮件（没发出去的另记，最近一次的原因也记下）、同时在线最多几台
export type DailyCounter = 'connections' | 'mails' | 'mailFailures';

export interface Stats {
    count(counter: DailyCounter): Promise<void>;
    // 邮件没发出去：记一次，留下这一次的原因
    mailFailed(reason: string): Promise<void>;
    close(): Promise<void>;
}

const PREFIX = 'bilin:stats:';
const KEEP_S = 40 * 24 * 3600;
const LAST_MAIL_FAILURE = 'bilin:mail:lastFailure';

// 服务器的本地日期，YYYY-MM-DD
export function localDay(date: Date): string {
    const pad = (n: number) => String(n).padStart(2, '0');
    return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}`;
}

export class RedisStats implements Stats {
    readonly #redis: Redis;

    constructor(url: string) {
        this.#redis = new Redis(url);
    }

    async count(counter: DailyCounter): Promise<void> {
        const key = PREFIX + localDay(new Date());
        await this.#redis.multi().hincrby(key, counter, 1).expire(key, KEEP_S).exec();
    }

    async mailFailed(reason: string): Promise<void> {
        await this.count('mailFailures');
        await this.#redis.set(LAST_MAIL_FAILURE, JSON.stringify({ at: new Date().toISOString(), reason: reason.slice(0, 300) }),
                              'EX', KEEP_S);
    }

    async close(): Promise<void> {
        await this.#redis.quit();
    }
}

// ---- 管理后台读的 ----

export interface DayStats {
    day: string;
    connections: number;
    mails: number;
    mailFailures: number;
    peakOnline: number;
}

// 记下现在在线几台：比今天记着的多就换上（管理后台每分钟记一次）
const PEAK = `
local current = tonumber(redis.call('HGET', KEYS[1], 'peakOnline') or '-1')
if tonumber(ARGV[1]) > current then redis.call('HSET', KEYS[1], 'peakOnline', ARGV[1]) end
redis.call('EXPIRE', KEYS[1], ARGV[2])`;

export async function recordOnline(redis: Redis, online: number, now = new Date()): Promise<void> {
    await redis.eval(PEAK, 1, PREFIX + localDay(now), online, KEEP_S);
}

// 最近 days 天，今天在最后
export async function recentDays(redis: Redis, days: number, now = new Date()): Promise<DayStats[]> {
    const dates = Array.from({ length: days }, (_, i) => {
        const date = new Date(now);
        date.setDate(now.getDate() - (days - 1 - i));
        return localDay(date);
    });
    const values = await Promise.all(dates.map((day) => redis.hgetall(PREFIX + day)));
    return dates.map((day, i) => {
        const value = values[i] ?? {};
        const number = (field: string) => Number(value[field] ?? 0);
        return {
            day, connections: number('connections'), mails: number('mails'), mailFailures: number('mailFailures'),
            peakOnline: number('peakOnline'),
        };
    });
}

export async function lastMailFailure(redis: Redis): Promise<{ at: string; reason: string } | null> {
    const value = await redis.get(LAST_MAIL_FAILURE);
    return value ? JSON.parse(value) as { at: string; reason: string } : null;
}
