import { randomBytes } from 'node:crypto';

import { Redis } from 'ioredis';

// 网页版经转发进程连电脑（docs/web.md）的一次性通行证：服务端发给浏览器（view）和那台电脑（share），
// 转发进程收下就作废。写明哪个房间（被连的设备 ID）、什么角色。TICKET_TTL_S 秒内不用也作废。
// 服务端和转发进程是两个进程，经 Redis 共用
export const TICKET_TTL_S = 60;
export const TICKET = /^[A-Za-z0-9_-]{43}$/;

export type TicketRole = 'share' | 'view';

export interface Ticket {
    room: string;
    role: TicketRole;
}

export interface Tickets {
    issue(ticket: Ticket): Promise<string>;
    // 收下：作废，返回写着的；没有、过期了、用过了：null
    redeem(ticket: string): Promise<Ticket | null>;
    close(): Promise<void>;
}

export function newTicket(): string {
    return randomBytes(32).toString('base64url');
}

const PREFIX = 'bilin:ticket:';

export class RedisTickets implements Tickets {
    readonly #redis: Redis;

    constructor(url: string) {
        this.#redis = new Redis(url);
    }

    async issue(ticket: Ticket): Promise<string> {
        const value = newTicket();
        await this.#redis.set(PREFIX + value, JSON.stringify(ticket), 'EX', TICKET_TTL_S);
        return value;
    }

    async redeem(ticket: string): Promise<Ticket | null> {
        if (!TICKET.test(ticket))
            return null;
        const stored = await this.#redis.getdel(PREFIX + ticket);
        return stored ? JSON.parse(stored) as Ticket : null;
    }

    async close(): Promise<void> {
        await this.#redis.quit();
    }
}
