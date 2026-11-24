import { randomBytes } from 'node:crypto';

import type { Collection, Filter } from 'mongodb';

import type { ConnectionReport, ReportPath, ReportResult, ReportRoute } from './protocol.js';
import { localDay } from './stats.js';

// 连接统计（docs/connection-stats.md）：去连别的设备的一方每次连接结束发来一条（protocol.ts 的 ConnectionReport），
// 服务器补上两边的系统、版本记下来，管理后台里看记录和统计。CONNECTIONS_TTL_S 以后由 MongoDB 自己删掉（隐私政策里这么说的）
export const CONNECTIONS_TTL_S = 90 * 24 * 60 * 60;

// 一条记录：去连的一方（viewer）和被连的一方（peer：按局域网地址连的没有 ID，系统、版本也就不知道，都是空的）
export interface ConnectionRecord {
    id: string;
    createdAt: string;
    viewer: { deviceId: string; platform: string; version: string };
    peer: { deviceId: string; platform: string; version: string };
    access: ConnectionReport['access'];
    result: ReportResult;
    reason: string;
    routes: ReportRoute[];
    path: ReportPath | '';
    connectMs: number;
    durationS: number;
    reconnects: number;
    approval: boolean;
    control: boolean;
    codec: string;
    resolution: string;
    fps: number;
    mbps: number;
    latencyMs: number;
}

export type NewConnection = Omit<ConnectionRecord, 'id' | 'createdAt'>;

// 列表的筛选：哪台设备的（去连、被连都算）、什么结果
export interface ConnectionFilter {
    deviceId?: string;
    result?: ReportResult;
}

export interface ConnectionStore {
    add(connection: NewConnection): Promise<void>;
    // 新的在前，最多 limit 条
    list(filter: ConnectionFilter, limit: number): Promise<ConnectionRecord[]>;
    // since 以后的（统计用）
    since(date: Date): Promise<ConnectionRecord[]>;
}

export function newConnectionId(): string {
    return randomBytes(8).toString('hex');
}

export interface ConnectionDocument extends Omit<ConnectionRecord, 'id' | 'createdAt'> {
    _id: string;
    createdAt: Date;
}

function record({ _id, createdAt, ...rest }: ConnectionDocument): ConnectionRecord {
    return { id: _id, createdAt: createdAt.toISOString(), ...rest };
}

export class MongoConnections implements ConnectionStore {
    readonly #connections: Collection<ConnectionDocument>;

    constructor(connections: Collection<ConnectionDocument>) {
        this.#connections = connections;
    }

    // 启动时建好：到期的由 MongoDB 自己删掉；按设备找
    async prepare(): Promise<void> {
        await this.#connections.createIndex({ createdAt: 1 }, { expireAfterSeconds: CONNECTIONS_TTL_S });
        await this.#connections.createIndex({ 'viewer.deviceId': 1, createdAt: -1 });
        await this.#connections.createIndex({ 'peer.deviceId': 1, createdAt: -1 });
    }

    async add(connection: NewConnection): Promise<void> {
        await this.#connections.insertOne({ _id: newConnectionId(), createdAt: new Date(), ...connection });
    }

    async list(filter: ConnectionFilter, limit: number): Promise<ConnectionRecord[]> {
        const query: Filter<ConnectionDocument> = {};
        if (filter.deviceId)
            query.$or = [{ 'viewer.deviceId': filter.deviceId }, { 'peer.deviceId': filter.deviceId }];
        if (filter.result)
            query.result = filter.result;
        const documents = await this.#connections.find(query).sort({ createdAt: -1 }).limit(limit).toArray();
        return documents.map(record);
    }

    async since(date: Date): Promise<ConnectionRecord[]> {
        const documents = await this.#connections.find({ createdAt: { $gte: date } }).toArray();
        return documents.map(record);
    }
}

// ---- 管理后台的统计 ----

export interface Count {
    key: string;
    count: number;
}

export interface DayConnections {
    day: string;
    attempts: number;
    connected: number;
}

export interface ConnectionSummary {
    days: number;
    // 去连的次数（用户关掉的、还没连上的不算）、连上了的、用了多少台设备去连
    attempts: number;
    connected: number;
    cancelled: number;
    viewers: number;
    // 连上了的：从开始连到连上（毫秒，中位数、九成以内），连了多久（秒，中位数、合计）
    connectMsMedian: number | null;
    connectMsP90: number | null;
    durationSMedian: number | null;
    durationSTotal: number;
    // 连上了的走哪条路；去连时试到了哪一步（direct、relay、relayTls 各有几次用上了）
    paths: Count[];
    routes: Count[];
    // 没连上的原因；连上以后断开的原因（用户自己断开的没有）
    failures: Count[];
    endings: Count[];
    // 哪种系统连哪种系统（"Windows → macOS"）、我的设备还是临时密码、按 ID 还是局域网地址
    platforms: Count[];
    access: Count[];
    // 各版本去连的次数和连上的次数
    versions: { version: string; attempts: number; connected: number }[];
    // 连着时：等过允许、控制过、重连过的次数；编码格式
    approval: number;
    control: number;
    reconnected: number;
    codecs: Count[];
    // 每天，最早的在前
    daily: DayConnections[];
}

function counts(values: string[]): Count[] {
    const map = new Map<string, number>();
    for (const value of values)
        map.set(value, (map.get(value) ?? 0) + 1);
    return [...map].map(([key, count]) => ({ key, count })).sort((a, b) => b.count - a.count || a.key.localeCompare(b.key));
}

function quantile(values: number[], q: number): number | null {
    if (values.length === 0)
        return null;
    const sorted = [...values].sort((a, b) => a - b);
    return sorted[Math.min(sorted.length - 1, Math.floor(q * sorted.length))] ?? null;
}

// 最近 days 天（今天在内，按服务器的本地日期）的记录汇总成管理后台的统计
export function summarize(records: ConnectionRecord[], days: number, now = new Date()): ConnectionSummary {
    const attempts = records.filter((r) => r.result !== 'cancelled');
    const connected = attempts.filter((r) => r.result === 'connected');
    const dayList = Array.from({ length: days }, (_, i) => {
        const date = new Date(now);
        date.setDate(now.getDate() - (days - 1 - i));
        return localDay(date);
    });
    const daily = new Map(dayList.map((day) => [day, { day, attempts: 0, connected: 0 }]));
    for (const r of attempts) {
        const day = daily.get(localDay(new Date(r.createdAt)));
        if (day) {
            day.attempts += 1;
            day.connected += r.result === 'connected' ? 1 : 0;
        }
    }
    const versions = new Map<string, { version: string; attempts: number; connected: number }>();
    for (const r of attempts) {
        const entry = versions.get(r.viewer.version) ?? { version: r.viewer.version, attempts: 0, connected: 0 };
        entry.attempts += 1;
        entry.connected += r.result === 'connected' ? 1 : 0;
        versions.set(r.viewer.version, entry);
    }
    return {
        days,
        attempts: attempts.length,
        connected: connected.length,
        cancelled: records.length - attempts.length,
        viewers: new Set(attempts.map((r) => r.viewer.deviceId)).size,
        connectMsMedian: quantile(connected.map((r) => r.connectMs), 0.5),
        connectMsP90: quantile(connected.map((r) => r.connectMs), 0.9),
        durationSMedian: quantile(connected.map((r) => r.durationS), 0.5),
        durationSTotal: connected.reduce((total, r) => total + r.durationS, 0),
        paths: counts(connected.map((r) => r.path)),
        routes: counts(connected.map((r) => r.routes.at(-1) ?? '')),
        failures: counts(attempts.filter((r) => r.result === 'failed').map((r) => r.reason)),
        endings: counts(connected.filter((r) => r.reason).map((r) => r.reason)),
        platforms: counts(attempts.map((r) => `${r.viewer.platform} → ${r.peer.platform || '局域网地址'}`)),
        access: counts(attempts.map((r) => r.peer.deviceId ? r.access : 'lan')),
        versions: [...versions.values()].sort((a, b) => b.attempts - a.attempts),
        approval: connected.filter((r) => r.approval).length,
        control: connected.filter((r) => r.control).length,
        reconnected: connected.filter((r) => r.reconnects > 0).length,
        codecs: counts(connected.filter((r) => r.codec).map((r) => r.codec)),
        daily: [...daily.values()],
    };
}
