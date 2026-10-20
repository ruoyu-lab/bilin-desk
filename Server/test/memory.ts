import { randomUUID } from 'node:crypto';

import { checkPassword, hashPassword, type Account, type Accounts } from '../src/accounts.js';
import type { Bus, BusMessage } from '../src/bus.js';
import {
    compressLogs, logsBytes, newFeedbackId, type FeedbackStore, type FeedbackSummary, type NewFeedback,
} from '../src/feedback.js';
import { CODE_TTL_S, codeHash, MAX_TRIES, newCode, type Codes } from '../src/codes.js';
import type { Limiter } from '../src/limiter.js';
import type { Mail, Mailer } from '../src/mail.js';
import type { CodePurpose, WakeAnswerResult, WakeInterface } from '../src/protocol.js';
import type { DailyCounter, Stats } from '../src/stats.js';
import { TICKET } from '../src/protocol.js';
import type { RelayLoad, RelayStatus } from '../src/relay-load.js';
import { newTicket, type Ticket, type Tickets } from '../src/tickets.js';
import type { Wakes } from '../src/wakes.js';
import { newWebSession, WEB_SESSION, type WebSessions } from '../src/web-sessions.js';
import type { ClaimResult, DeviceClaim, DeviceRecord, Registry } from '../src/registry.js';

function record(d: DeviceClaim & { wake?: WakeInterface[] }): DeviceRecord {
    return { id: d.id, name: d.name, platform: d.platform, publicKey: d.publicKey, machine: d.machine, wake: d.wake ?? [] };
}

// 测试用：内存里的设备登记表，规则和 MongoRegistry 一样（太久没上线的删掉由 MongoDB 做，这里只记时间）
export class MemoryRegistry implements Registry {
    readonly devices = new Map<string, DeviceClaim & { accountId: string | null; lastSeenAt: number; wake?: WakeInterface[] }>();

    async exists(id: string): Promise<boolean> {
        return this.devices.has(id);
    }

    async owns(id: string, tokenHash: string): Promise<boolean> {
        return this.devices.get(id)?.tokenHash === tokenHash;
    }

    async claim(device: DeviceClaim): Promise<ClaimResult> {
        const existing = this.devices.get(device.id);
        if (existing && existing.tokenHash !== device.tokenHash)
            return 'taken';
        this.devices.set(device.id, { ...device, accountId: existing?.accountId ?? null, lastSeenAt: Date.now() });
        return 'ok';
    }

    async seen(ids: string[]): Promise<void> {
        for (const id of ids) {
            const existing = this.devices.get(id);
            if (existing)
                existing.lastSeenAt = Date.now();
        }
    }

    async rename(id: string, name: string): Promise<void> {
        const existing = this.devices.get(id);
        if (existing)
            this.devices.set(id, { ...existing, name });
    }

    async bind(id: string, accountId: string | null): Promise<void> {
        const existing = this.devices.get(id);
        if (existing)
            this.devices.set(id, { ...existing, accountId, wake: [] });
    }

    async setWake(id: string, interfaces: WakeInterface[]): Promise<void> {
        const existing = this.devices.get(id);
        if (existing)
            this.devices.set(id, { ...existing, wake: interfaces });
    }

    async accountOf(id: string): Promise<string | null> {
        return this.devices.get(id)?.accountId ?? null;
    }

    async device(id: string): Promise<DeviceRecord | null> {
        const d = this.devices.get(id);
        return d ? record(d) : null;
    }

    async devicesOf(accountId: string): Promise<DeviceRecord[]> {
        return [...this.devices.values()]
            .filter((d) => d.accountId === accountId)
            .map(record);
    }

    async adopt(to: string, from: string): Promise<void> {
        const source = this.devices.get(from);
        const target = this.devices.get(to);
        if (!source || !target)
            return;
        this.devices.set(to, { ...source, id: to, accountId: target.accountId, lastSeenAt: Date.now() });
        this.devices.delete(from);
    }

    async unbindAll(accountId: string): Promise<void> {
        for (const device of this.devices.values()) {
            if (device.accountId === accountId) {
                device.accountId = null;
                device.wake = [];
            }
        }
    }
}

// 测试用：内存里的账号，规则和 MongoAccounts 一样
export class MemoryAccounts implements Accounts {
    readonly #accounts = new Map<string, Account & { passwordHash: string }>();

    async create(email: string, nickname: string, password: string): Promise<Account | 'emailTaken'> {
        if ([...this.#accounts.values()].some((a) => a.email === email))
            return 'emailTaken';
        const account = { id: randomUUID(), email, nickname, passwordHash: await hashPassword(password) };
        this.#accounts.set(account.id, account);
        return { id: account.id, email, nickname };
    }

    async verify(email: string, password: string): Promise<Account | null> {
        const account = [...this.#accounts.values()].find((a) => a.email === email);
        if (!account || !await checkPassword(password, account.passwordHash))
            return null;
        return { id: account.id, email: account.email, nickname: account.nickname };
    }

    async get(id: string): Promise<Account | null> {
        const account = this.#accounts.get(id);
        return account ? { id: account.id, email: account.email, nickname: account.nickname } : null;
    }

    async byEmail(email: string): Promise<Account | null> {
        const account = [...this.#accounts.values()].find((a) => a.email === email);
        return account ? { id: account.id, email: account.email, nickname: account.nickname } : null;
    }

    async checkPassword(id: string, password: string): Promise<boolean> {
        const account = this.#accounts.get(id);
        return account !== undefined && await checkPassword(password, account.passwordHash);
    }

    async setNickname(id: string, nickname: string): Promise<void> {
        const account = this.#accounts.get(id);
        if (account)
            account.nickname = nickname;
    }

    async setPassword(id: string, password: string): Promise<void> {
        const account = this.#accounts.get(id);
        if (account)
            account.passwordHash = await hashPassword(password);
    }

    async remove(id: string): Promise<void> {
        this.#accounts.delete(id);
    }

    get count(): number {
        return this.#accounts.size;
    }
}

// 测试用：内存里的限速计数，规则和 RedisLimiter 一样（窗从第一次数开始，到点清零）
export class MemoryLimiter implements Limiter {
    readonly #counts = new Map<string, { count: number; until: number }>();

    async hit(key: string, windowS: number): Promise<number> {
        const now = Date.now();
        const current = this.#counts.get(key);
        const next = current && current.until > now ? { count: current.count + 1, until: current.until }
                                                    : { count: 1, until: now + windowS * 1000 };
        this.#counts.set(key, next);
        return next.count;
    }

    async count(key: string): Promise<number> {
        const current = this.#counts.get(key);
        return current && current.until > Date.now() ? current.count : 0;
    }

    async close(): Promise<void> {}
}

// 测试用：几个"实例"共用的一个 Redis。消息异步送达（像经过网络），publish 返回订阅了的实例数
export class MemoryBroker {
    readonly #subscribers = new Map<string, Set<MemoryBus>>();
    readonly online = new Set<string>();

    bus(): MemoryBus {
        return new MemoryBus(this);
    }

    publish(deviceId: string, message: BusMessage): number {
        const buses = [...(this.#subscribers.get(deviceId) ?? [])];
        for (const bus of buses)
            setImmediate(() => bus.deliver(deviceId, structuredClone(message)));
        return buses.length;
    }

    subscribe(deviceId: string, bus: MemoryBus): void {
        let set = this.#subscribers.get(deviceId);
        if (!set) {
            set = new Set();
            this.#subscribers.set(deviceId, set);
        }
        set.add(bus);
    }

    unsubscribe(deviceId: string, bus: MemoryBus): void {
        this.#subscribers.get(deviceId)?.delete(bus);
    }
}

export class MemoryBus implements Bus {
    readonly #broker: MemoryBroker;
    #handler: (deviceId: string, message: BusMessage) => void = () => {};

    constructor(broker: MemoryBroker) {
        this.#broker = broker;
    }

    async publish(deviceId: string, message: BusMessage): Promise<number> {
        return this.#broker.publish(deviceId, message);
    }

    async subscribe(deviceId: string): Promise<void> {
        this.#broker.subscribe(deviceId, this);
    }

    async unsubscribe(deviceId: string): Promise<void> {
        this.#broker.unsubscribe(deviceId, this);
    }

    onMessage(handler: (deviceId: string, message: BusMessage) => void): void {
        this.#handler = handler;
    }

    async markOnline(deviceIds: string[]): Promise<void> {
        for (const id of deviceIds)
            this.#broker.online.add(id);
    }

    async markOffline(deviceId: string): Promise<void> {
        this.#broker.online.delete(deviceId);
    }

    async online(deviceIds: string[]): Promise<Set<string>> {
        return new Set(deviceIds.filter((id) => this.#broker.online.has(id)));
    }

    deliver(deviceId: string, message: BusMessage): void {
        this.#handler(deviceId, message);
    }

    async close(): Promise<void> {}
}

// 测试用：内存里的验证码，规则和 RedisCodes 一样
export class MemoryCodes implements Codes {
    readonly #codes = new Map<string, { hash: string; tries: number; until: number }>();

    async issue(purpose: CodePurpose, email: string): Promise<string> {
        const code = newCode();
        this.#codes.set(`${purpose}:${email}`, { hash: codeHash(purpose, email, code), tries: 0, until: Date.now() + CODE_TTL_S * 1000 });
        return code;
    }

    async check(purpose: CodePurpose, email: string, code: string): Promise<boolean> {
        const key = `${purpose}:${email}`;
        const stored = this.#codes.get(key);
        if (!stored || stored.until <= Date.now())
            return false;
        if (stored.hash === codeHash(purpose, email, code)) {
            this.#codes.delete(key);
            return true;
        }
        if (++stored.tries >= MAX_TRIES)
            this.#codes.delete(key);
        return false;
    }

    async close(): Promise<void> {}
}

// 测试用：不发邮件，记下来（测试从里面拿验证码）；failing 时发不出去
export class MemoryMailer implements Mailer {
    readonly mails: Mail[] = [];
    failing = false;

    async send(mail: Mail): Promise<void> {
        if (this.failing)
            throw new Error('发不出去');
        this.mails.push(mail);
    }

    // 最近发给这个邮箱的验证码
    codeFor(email: string): string {
        const mail = this.mails.filter((m) => m.to === email).at(-1);
        return /\d{6}/.exec(mail?.subject ?? '')?.[0] ?? '';
    }
}

// 测试用：内存里的每天的数目
export class MemoryStats implements Stats {
    readonly counts = new Map<DailyCounter, number>();
    lastMailFailure: string | null = null;

    async count(counter: DailyCounter): Promise<void> {
        this.counts.set(counter, (this.counts.get(counter) ?? 0) + 1);
    }

    async mailFailed(reason: string): Promise<void> {
        await this.count('mailFailures');
        this.lastMailFailure = reason;
    }

    async close(): Promise<void> {}
}

// 测试用：内存里的反馈，规则和 MongoFeedback 一样（到期删掉由 MongoDB 做）
export class MemoryFeedback implements FeedbackStore {
    readonly items: Array<FeedbackSummary & { logs: Buffer | null }> = [];
    #notify: string | null = null;

    async add(feedback: NewFeedback): Promise<FeedbackSummary> {
        const { logs, ...rest } = feedback;
        const item = {
            ...rest, id: newFeedbackId(), createdAt: new Date().toISOString(), logsBytes: logsBytes(logs), read: false,
            logs: await compressLogs(logs),
        };
        this.items.unshift(item);
        return MemoryFeedback.#summary(item);
    }

    async list(): Promise<FeedbackSummary[]> {
        return this.items.map(MemoryFeedback.#summary);
    }

    async markRead(id: string): Promise<FeedbackSummary | null> {
        const item = this.items.find((i) => i.id === id);
        if (!item)
            return null;
        item.read = true;
        return MemoryFeedback.#summary(item);
    }

    async logs(id: string): Promise<Buffer | null> {
        return this.items.find((i) => i.id === id)?.logs ?? null;
    }

    async remove(id: string): Promise<boolean> {
        const index = this.items.findIndex((i) => i.id === id);
        if (index < 0)
            return false;
        this.items.splice(index, 1);
        return true;
    }

    async notifyAddress(): Promise<string | null> {
        return this.#notify;
    }

    async setNotifyAddress(to: string | null): Promise<void> {
        this.#notify = to;
    }

    static #summary(item: FeedbackSummary & { logs: Buffer | null }): FeedbackSummary {
        const { logs: _, ...summary } = item;
        return summary;
    }
}

// 测试用：内存里的一次性通行证，规则和 RedisTickets 一样（过期由 Redis 做，这里不管）
export class MemoryTickets implements Tickets {
    readonly #tickets = new Map<string, Ticket>();

    async issue(ticket: Ticket): Promise<string> {
        const value = newTicket();
        this.#tickets.set(value, ticket);
        return value;
    }

    async redeem(ticket: string): Promise<Ticket | null> {
        if (!TICKET.test(ticket))
            return null;
        const stored = this.#tickets.get(ticket) ?? null;
        this.#tickets.delete(ticket);
        return stored;
    }

    readonly #answers = new Map<string, string>();

    async answer(ticket: string, reason: string): Promise<void> {
        this.#answers.set(ticket, reason);
    }

    async answered(ticket: string): Promise<string | null> {
        return this.#answers.get(ticket) ?? null;
    }

    async close(): Promise<void> {}
}

// 测试用：帮忙叫醒的电脑的回答
export class MemoryWakes implements Wakes {
    readonly #answers = new Map<string, Map<string, WakeAnswerResult>>();

    async answer(request: string, helper: string, result: WakeAnswerResult): Promise<void> {
        const answers = this.#answers.get(request) ?? new Map<string, WakeAnswerResult>();
        answers.set(helper, result);
        this.#answers.set(request, answers);
    }

    async answers(request: string): Promise<Map<string, WakeAnswerResult>> {
        return new Map(this.#answers.get(request));
    }

    async close(): Promise<void> {}
}

// 测试用：转发进程报的网页版路数
export class MemoryRelayStatus implements RelayStatus {
    current: RelayLoad | null = { viewers: 0, max: 20 };

    async setLoad(load: RelayLoad): Promise<void> {
        this.current = load;
    }

    async load(): Promise<RelayLoad | null> {
        return this.current;
    }

    async close(): Promise<void> {}
}

// 测试用：网页版登录着的账号
export class MemoryWebSessions implements WebSessions {
    readonly #sessions = new Map<string, string>();

    async create(accountId: string): Promise<string> {
        const token = newWebSession();
        this.#sessions.set(token, accountId);
        return token;
    }

    async account(token: string): Promise<string | null> {
        return WEB_SESSION.test(token) ? this.#sessions.get(token) ?? null : null;
    }

    async remove(token: string): Promise<void> {
        this.#sessions.delete(token);
    }

    async close(): Promise<void> {}
}
