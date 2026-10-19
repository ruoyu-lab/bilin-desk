import { randomBytes } from 'node:crypto';

import { Redis } from 'ioredis';

import { TICKET } from './protocol.js';

// 网页版经转发进程连电脑（docs/web.md）的一次性通行证：服务端发给浏览器（view）和那台电脑（share），
// 转发进程收下就作废。写明哪个房间（被连的设备 ID，我的设备是 <ID>/mine）、什么角色。TICKET_TTL_S 秒内不用也作废。
// 电脑拿到它的那张以后回答准备好了没有（answer）：进了房间、换好了钥匙，或者为什么不行；服务端等到回答才把浏览器的
// 那张交出去。服务端（几个实例）和转发进程经 Redis 共用
export const TICKET_TTL_S = 60;

export type TicketRole = 'share' | 'view';

export interface Ticket {
    room: string;
    role: TicketRole;
}

export interface Tickets {
    issue(ticket: Ticket): Promise<string>;
    // 收下：作废，返回写着的；没有、过期了、用过了：null
    redeem(ticket: string): Promise<Ticket | null>;
    // 电脑对它那张的回答：reason 空的是准备好了，不然是为什么不行（rc/peer_protocol.h 的原因）
    answer(ticket: string, reason: string): Promise<void>;
    // 还没回答：null
    answered(ticket: string): Promise<string | null>;
    close(): Promise<void>;
}

export function newTicket(): string {
    return randomBytes(32).toString('base64url');
}

const PREFIX = 'bilin:ticket:';
const ANSWER = 'bilin:ticket-answer:';

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

    async answer(ticket: string, reason: string): Promise<void> {
        await this.#redis.set(ANSWER + ticket, reason, 'EX', TICKET_TTL_S);
    }

    async answered(ticket: string): Promise<string | null> {
        return this.#redis.get(ANSWER + ticket);
    }

    async close(): Promise<void> {
        await this.#redis.quit();
    }
}
