import { randomBytes } from 'node:crypto';
import { promisify } from 'node:util';
import { gunzip as gunzipCallback, gzip as gzipCallback } from 'node:zlib';

import { Binary, type Collection } from 'mongodb';

import { DEVICE_ID, MAX_VERSION, TOKEN } from './protocol.js';

const gzip = promisify(gzipCallback);
const gunzip = promisify(gunzipCallback);

// 用户在程序里提交的反馈（docs/feedback.md）：说了什么、程序的日志、版本和系统，联系方式（登录了是账号的邮箱）。
// 管理后台里看。FEEDBACK_TTL_S 以后由 MongoDB 自己删掉（隐私政策里这么说的）
export const FEEDBACK_TTL_S = 90 * 24 * 60 * 60;
export const MAX_FEEDBACK_TEXT = 5000;
export const MAX_CONTACT = 100;
export const MAX_SYSTEM = 100;
// 日志不压缩时最多这么大（程序只带最近的一段）
export const MAX_LOGS_BYTES = 6 * 1024 * 1024;
const MAX_LOG_FILES = 8;
const MAX_LOG_NAME = 64;
const FEEDBACK_ID = /^[0-9a-f]{16}$/;

// POST /feedback 的请求体（程序的后台服务发）
export interface FeedbackRequest {
    deviceId: string;
    token: string;
    version: string;
    system: string;
    contact: string;
    text: string;
    logs: FeedbackLog[];
}

export interface FeedbackLog {
    name: string;
    text: string;
}

export interface NewFeedback {
    deviceId: string;
    deviceName: string;
    platform: string;
    version: string;
    system: string;
    // 账号的邮箱，或者没登录时自己填的（可以不填：空的）
    contact: string;
    signedIn: boolean;
    text: string;
    logs: FeedbackLog[];
}

// 列表里的一条
export interface FeedbackSummary {
    id: string;
    createdAt: string;
    deviceId: string;
    deviceName: string;
    platform: string;
    version: string;
    system: string;
    contact: string;
    signedIn: boolean;
    text: string;
    // 日志不压缩时多大；0 是没带
    logsBytes: number;
    read: boolean;
}

export interface FeedbackStore {
    add(feedback: NewFeedback): Promise<FeedbackSummary>;
    // 新的在前
    list(): Promise<FeedbackSummary[]>;
    // 看过了
    markRead(id: string): Promise<FeedbackSummary | null>;
    // 日志（gzip 压缩的文本）；没有这条、没带日志时 null
    logs(id: string): Promise<Buffer | null>;
    remove(id: string): Promise<boolean>;
    // 有新的反馈时发邮件告诉谁；null 是不发
    notifyAddress(): Promise<string | null>;
    setNotifyAddress(to: string | null): Promise<void>;
}

function isRecord(value: unknown): value is Record<string, unknown> {
    return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function bounded(value: unknown, min: number, max: number): value is string {
    return typeof value === 'string' && value.length >= min && value.length <= max;
}

function isLog(value: unknown): value is FeedbackLog {
    return isRecord(value) && bounded(value.name, 1, MAX_LOG_NAME) && typeof value.text === 'string';
}

// 不对的（缺了、太长、日志太大）：null
export function parseFeedback(body: unknown): FeedbackRequest | null {
    if (!isRecord(body))
        return null;
    const { deviceId, token, version, system, logs } = body;
    const text = typeof body.text === 'string' ? body.text.trim() : null;
    const contact = typeof body.contact === 'string' ? body.contact.trim() : null;
    if (typeof deviceId !== 'string' || !DEVICE_ID.test(deviceId) || typeof token !== 'string' || !TOKEN.test(token)
        || !bounded(version, 1, MAX_VERSION) || !bounded(system, 1, MAX_SYSTEM)
        || text === null || !bounded(text, 1, MAX_FEEDBACK_TEXT) || contact === null || !bounded(contact, 0, MAX_CONTACT)
        || !Array.isArray(logs) || logs.length > MAX_LOG_FILES || !logs.every(isLog) || logsBytes(logs) > MAX_LOGS_BYTES)
        return null;
    return { deviceId, token, version, system, contact, text, logs: logs.map((log) => ({ name: log.name, text: log.text })) };
}

export function isFeedbackId(id: string): boolean {
    return FEEDBACK_ID.test(id);
}

export function newFeedbackId(): string {
    return randomBytes(8).toString('hex');
}

// 几份日志接成一份文本（下载下来就是这样）
export function joinLogs(logs: FeedbackLog[]): string {
    return logs.map((log) => `===== ${log.name} =====\n${log.text}${log.text.endsWith('\n') ? '' : '\n'}`).join('\n');
}

export function logsBytes(logs: FeedbackLog[]): number {
    return logs.reduce((total, log) => total + Buffer.byteLength(log.text), 0);
}

export async function compressLogs(logs: FeedbackLog[]): Promise<Buffer | null> {
    return logs.length > 0 ? gzip(joinLogs(logs)) : null;
}

export async function decompressLogs(compressed: Buffer): Promise<string> {
    return (await gunzip(compressed)).toString();
}

export interface FeedbackDocument {
    _id: string;
    deviceId: string;
    deviceName: string;
    platform: string;
    version: string;
    system: string;
    contact: string;
    signedIn: boolean;
    text: string;
    logs: Binary | null;
    logsBytes: number;
    read: boolean;
    createdAt: Date;
}

export interface SettingsDocument {
    _id: string;
    notify: string | null;
}

function summary(d: Omit<FeedbackDocument, 'logs'>): FeedbackSummary {
    return {
        id: d._id, createdAt: d.createdAt.toISOString(), deviceId: d.deviceId, deviceName: d.deviceName,
        platform: d.platform, version: d.version, system: d.system, contact: d.contact, signedIn: d.signedIn,
        text: d.text, logsBytes: d.logsBytes, read: d.read,
    };
}

export class MongoFeedback implements FeedbackStore {
    readonly #feedback: Collection<FeedbackDocument>;
    readonly #settings: Collection<SettingsDocument>;

    constructor(feedback: Collection<FeedbackDocument>, settings: Collection<SettingsDocument>) {
        this.#feedback = feedback;
        this.#settings = settings;
    }

    // 启动时建好：到期的由 MongoDB 自己删掉
    async prepare(): Promise<void> {
        await this.#feedback.createIndex({ createdAt: 1 }, { expireAfterSeconds: FEEDBACK_TTL_S });
    }

    async add(feedback: NewFeedback): Promise<FeedbackSummary> {
        const compressed = await compressLogs(feedback.logs);
        const document: FeedbackDocument = {
            _id: newFeedbackId(),
            deviceId: feedback.deviceId,
            deviceName: feedback.deviceName,
            platform: feedback.platform,
            version: feedback.version,
            system: feedback.system,
            contact: feedback.contact,
            signedIn: feedback.signedIn,
            text: feedback.text,
            logs: compressed ? new Binary(compressed) : null,
            logsBytes: logsBytes(feedback.logs),
            read: false,
            createdAt: new Date(),
        };
        await this.#feedback.insertOne(document);
        return summary(document);
    }

    async list(): Promise<FeedbackSummary[]> {
        const documents = await this.#feedback.find({}, { projection: { logs: 0 } }).sort({ createdAt: -1 }).toArray();
        return documents.map(summary);
    }

    async markRead(id: string): Promise<FeedbackSummary | null> {
        const document = await this.#feedback.findOneAndUpdate(
            { _id: id }, { $set: { read: true } }, { projection: { logs: 0 }, returnDocument: 'after' });
        return document ? summary(document) : null;
    }

    async logs(id: string): Promise<Buffer | null> {
        const document = await this.#feedback.findOne({ _id: id }, { projection: { logs: 1 } });
        return document?.logs ? Buffer.from(document.logs.buffer) : null;
    }

    async remove(id: string): Promise<boolean> {
        return (await this.#feedback.deleteOne({ _id: id })).deletedCount === 1;
    }

    async notifyAddress(): Promise<string | null> {
        return (await this.#settings.findOne({ _id: 'feedback' }))?.notify ?? null;
    }

    async setNotifyAddress(to: string | null): Promise<void> {
        await this.#settings.updateOne({ _id: 'feedback' }, { $set: { notify: to } }, { upsert: true });
    }
}
