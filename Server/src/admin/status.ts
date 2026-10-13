import { execFile } from 'node:child_process';
import { readFile, statfs } from 'node:fs/promises';
import { cpus, loadavg, release, uptime } from 'node:os';
import { connect } from 'node:tls';
import { promisify } from 'node:util';

import type { Db } from 'mongodb';
import { Redis } from 'ioredis';

import { lastMailFailure, recentDays, type DayStats } from '../stats.js';
import type { Backup } from './backups.js';

// 管理后台首页看的：服务器和服务端在不在正常运行。只有数目，不看任何一个账号、设备
export interface Status {
    uptimeS: number;
    memory: { totalBytes: number; availableBytes: number };
    disk: { totalBytes: number; availableBytes: number };
    databaseBytes: number;
    devices: { registered: number; online: number };
    // 最近 30 天上过线的设备，按平台和程序的版本数（看新版本装上了多少）
    versions: Array<{ platform: string; version: string; devices: number }>;
    accounts: number;
    // 各个服务在不在运行：mongod、redis、nginx、coturn 看 systemd，服务端看它的 /health
    services: Record<string, boolean>;
    // 系统、几个核、最近 1、5、15 分钟的平均负载，各个软件的版本
    system: { os: string; kernel: string; cpus: number; load: number[]; node: string; mongodb: string; redis: string };
    // 网站的证书什么时候到期（validTo 为 null：问不到）；null：没配（开发用的虚拟机）
    certificate: { host: string; validTo: string | null } | null;
    // 最新的一份备份
    backup: Backup | null;
    // 验证码邮件：怎么发（resend：经 Resend | log：写进日志，开发用的虚拟机 | none：没配），今天发了几封、一天最多几封，
    // 最近一次没发出去的原因
    mail: { mode: MailMode; today: number; limit: number; lastFailure: { at: string; reason: string } | null };
    // 最近 7 天每天的数目，今天在最后
    trends: Array<{ day: string; devices: number; accounts: number; connections: number; peakOnline: number }>;
}

export type MailMode = 'resend' | 'log' | 'none';

const TREND_DAYS = 7;
// 服务器的时区：按它分天（和 stats.ts 一样按服务器的本地日期）
const TIME_ZONE = Intl.DateTimeFormat().resolvedOptions().timeZone;

const run = promisify(execFile);

// Linux 的 /proc/meminfo：MemAvailable 算上了能腾出来的缓存（MemFree 不算，小机器上总是很少）
async function memory(): Promise<Status['memory']> {
    const info = await readFile('/proc/meminfo', 'utf8');
    const field = (name: string) => Number(new RegExp(`^${name}:\\s+(\\d+) kB`, 'm').exec(info)?.[1] ?? 0) * 1024;
    return { totalBytes: field('MemTotal'), availableBytes: field('MemAvailable') };
}

async function disk(directory: string): Promise<Status['disk']> {
    const info = await statfs(directory);
    return { totalBytes: info.blocks * info.bsize, availableBytes: info.bavail * info.bsize };
}

async function systemdActive(unit: string): Promise<boolean> {
    try {
        await run('systemctl', ['is-active', '--quiet', unit]);
        return true;
    } catch {
        return false;
    }
}

async function healthy(url: string): Promise<boolean> {
    try {
        return (await fetch(url, { signal: AbortSignal.timeout(3000) })).ok;
    } catch {
        return false;
    }
}

async function versions(db: Db): Promise<Status['versions']> {
    const since = new Date(Date.now() - 30 * 24 * 3600 * 1000);
    const groups = await db.collection('devices').aggregate<{ _id: { platform: string; version: string }; devices: number }>([
        { $match: { lastSeenAt: { $gte: since } } },
        { $group: { _id: { platform: '$platform', version: '$version' }, devices: { $sum: 1 } } },
        { $sort: { devices: -1 } },
    ]).toArray();
    return groups.map((g) => ({ platform: g._id.platform, version: g._id.version, devices: g.devices }));
}

// 最近 days 天每天新建了几个（设备、账号都记着 createdAt）
async function createdPerDay(db: Db, collection: string, days: number): Promise<Map<string, number>> {
    const since = new Date();
    since.setHours(0, 0, 0, 0);
    since.setDate(since.getDate() - (days - 1));
    const groups = await db.collection(collection).aggregate<{ _id: string; count: number }>([
        { $match: { createdAt: { $gte: since } } },
        { $group: { _id: { $dateToString: { format: '%Y-%m-%d', date: '$createdAt', timezone: TIME_ZONE } }, count: { $sum: 1 } } },
    ]).toArray();
    return new Map(groups.map((g) => [g._id, g.count]));
}

// days：stats.ts 记的每天的数目；新设备、新账号从数据库里数
async function trends(db: Db, days: DayStats[]): Promise<Status['trends']> {
    const [devices, accounts] = await Promise.all([
        createdPerDay(db, 'devices', days.length), createdPerDay(db, 'accounts', days.length),
    ]);
    return days.map((d) => ({
        day: d.day, devices: devices.get(d.day) ?? 0, accounts: accounts.get(d.day) ?? 0, connections: d.connections,
        peakOnline: d.peakOnline,
    }));
}

async function osName(): Promise<string> {
    try {
        return /^PRETTY_NAME="?([^"\n]*)"?/m.exec(await readFile('/etc/os-release', 'utf8'))?.[1] ?? '';
    } catch {
        return '';
    }
}

async function system(db: Db, redis: Redis): Promise<Status['system']> {
    const [os, build, info] = await Promise.all([osName(), db.command({ buildInfo: 1 }), redis.info('server')]);
    return {
        os, kernel: release(), cpus: cpus().length, load: loadavg().map((n) => Math.round(n * 100) / 100),
        node: process.version, mongodb: String(build.version ?? ''), redis: /^redis_version:(\S+)/m.exec(info)?.[1] ?? '',
    };
}

// 本机 Nginx 给 host 用的证书什么时候到期：经 TLS 连一下看（证书文件只有 root 读得了）
function certificateExpiry(host: string): Promise<string | null> {
    return new Promise((resolve) => {
        const socket = connect({ host: '127.0.0.1', port: 443, servername: host, rejectUnauthorized: false, timeout: 3000 }, () => {
            const validTo = socket.getPeerCertificate().valid_to;
            socket.end();
            resolve(validTo ? new Date(validTo).toISOString() : null);
        });
        socket.on('error', () => resolve(null));
        socket.on('timeout', () => {
            socket.destroy();
            resolve(null);
        });
    });
}

// 在线的设备：服务端各个实例往 Redis 里报的（bus.ts 的 bilin:presence:）
export async function onlineDevices(redis: Redis): Promise<number> {
    let count = 0;
    let cursor = '0';
    do {
        const [next, keys] = await redis.scan(cursor, 'MATCH', 'bilin:presence:*', 'COUNT', 1000);
        cursor = next;
        count += keys.length;
    } while (cursor !== '0');
    return count;
}

export interface StatusSources {
    db: Db;
    redis: Redis;
    // 数据所在的盘（备份、安装包放在这里）
    dataDirectory: string;
    // 服务端的 /health
    healthUrl: string;
    // 网站的域名：看它的证书；null：不看（开发用的虚拟机）
    certificateHost: string | null;
    latestBackup: () => Promise<Backup | null>;
    // 验证码邮件现在怎么发（密钥可能刚设上，每次都看）
    mailMode: () => Promise<MailMode>;
    // 服务端一天最多发几封
    mailLimit: number;
}

export function systemStatus(sources: StatusSources): () => Promise<Status> {
    return async () => {
        const units = ['mongod', 'redis', 'nginx', 'coturn'];
        const host = sources.certificateHost;
        const counts = await recentDays(sources.redis, TREND_DAYS);
        const [details, days, validTo, backup, mailMode, failure] = await Promise.all([
            system(sources.db, sources.redis),
            trends(sources.db, counts),
            host ? certificateExpiry(host) : Promise.resolve(null),
            sources.latestBackup(),
            sources.mailMode(),
            lastMailFailure(sources.redis),
        ]);
        const [mem, space, stats, registered, byVersion, accounts, online, server, ...active] = await Promise.all([
            memory(),
            disk(sources.dataDirectory),
            sources.db.stats(),
            sources.db.collection('devices').estimatedDocumentCount(),
            versions(sources.db),
            sources.db.collection('accounts').estimatedDocumentCount(),
            onlineDevices(sources.redis),
            healthy(sources.healthUrl),
            ...units.map(systemdActive),
        ]);
        const services: Record<string, boolean> = { server };
        units.forEach((unit, i) => {
            services[unit] = active[i] ?? false;
        });
        return {
            uptimeS: Math.round(uptime()),
            memory: mem,
            disk: space,
            databaseBytes: Number(stats.storageSize ?? 0) + Number(stats.indexSize ?? 0),
            devices: { registered, online },
            versions: byVersion,
            accounts,
            services,
            system: details,
            certificate: host ? { host, validTo } : null,
            backup,
            mail: { mode: mailMode, today: counts.at(-1)?.mails ?? 0, limit: sources.mailLimit, lastFailure: failure },
            trends: days,
        };
    };
}
