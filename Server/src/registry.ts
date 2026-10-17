import { createHash } from 'node:crypto';
import { type Collection, MongoServerError } from 'mongodb';

export interface DeviceClaim {
    id: string;
    tokenHash: string;
    name: string;
    platform: string;
    // 设备上程序的版本（管理后台看新版本装上了多少）
    version: string;
    // 电脑的硬件编号的哈希（protocol.ts 的 MACHINE）；没有时为空
    machine: string;
    // 设备密钥的公钥（base64）：同一账号的设备之间、信任它的设备用它认出它
    publicKey: string;
}

// 账号下的一台设备
export interface DeviceRecord {
    id: string;
    name: string;
    platform: string;
    publicKey: string;
    machine: string;
}

// ok：这个 ID 是它的（第一次登记，或者令牌对得上）；taken：已经属于别的设备
export type ClaimResult = 'ok' | 'taken';

// 登记过的设备。ID 属于第一个拿令牌来登记它的设备，以后要拿同一个令牌才算是它。
// 不用登录：令牌就是"这个 ID 是我的"的凭据。登录了账号的设备挂在账号下（一台设备同一时间一个账号）。
// FORGET_AFTER_S 没上线的设备记录删掉（隐私政策里这么说的）：每次上线、连着的时候隔一阵记一次
export interface Registry {
    // 登记过没有
    exists(id: string): Promise<boolean>;
    // 这个 ID 登记过、令牌对得上（不在长连接上的请求，比如提交反馈，拿它认设备）
    owns(id: string, tokenHash: string): Promise<boolean>;
    claim(device: DeviceClaim): Promise<ClaimResult>;
    // 这些设备连着（还在用）
    seen(ids: string[]): Promise<void>;
    rename(id: string, name: string): Promise<void>;
    // 挂到账号下；null：退出账号
    bind(id: string, accountId: string | null): Promise<void>;
    accountOf(id: string): Promise<string | null>;
    device(id: string): Promise<DeviceRecord | null>;
    devicesOf(accountId: string): Promise<DeviceRecord[]>;
    // 账号注销了：挂在下面的设备都拿下来
    unbindAll(accountId: string): Promise<void>;
    // 同一台电脑重装了（hub.ts 的 signIn）：from 的令牌、公钥、名字这些换到 to 上（to 还在原来的账号下），from 删掉
    adopt(to: string, from: string): Promise<void>;
}

export const FORGET_AFTER_S = 180 * 24 * 60 * 60;

export function hashToken(token: string): string {
    return createHash('sha256').update(token).digest('hex');
}

export interface DeviceDocument {
    _id: string;
    tokenHash: string;
    name: string;
    platform: string;
    version: string;
    machine: string;
    publicKey: string;
    accountId: string | null;
    createdAt: Date;
    lastSeenAt: Date;
}

const DUPLICATE_KEY = 11000;

export class MongoRegistry implements Registry {
    readonly #devices: Collection<DeviceDocument>;

    constructor(devices: Collection<DeviceDocument>) {
        this.#devices = devices;
    }

    // 启动时建好：按账号找设备；太久没上线的由 MongoDB 自己删掉
    async prepare(): Promise<void> {
        await this.#devices.createIndex({ accountId: 1 });
        await this.#devices.createIndex({ lastSeenAt: 1 }, { expireAfterSeconds: FORGET_AFTER_S });
    }

    async exists(id: string): Promise<boolean> {
        return (await this.#devices.countDocuments({ _id: id }, { limit: 1 })) > 0;
    }

    async owns(id: string, tokenHash: string): Promise<boolean> {
        return (await this.#devices.countDocuments({ _id: id, tokenHash }, { limit: 1 })) > 0;
    }

    async seen(ids: string[]): Promise<void> {
        if (ids.length > 0)
            await this.#devices.updateMany({ _id: { $in: ids } }, { $set: { lastSeenAt: new Date() } });
    }

    async claim(device: DeviceClaim): Promise<ClaimResult> {
        const now = new Date();
        try {
            await this.#devices.insertOne({
                _id: device.id,
                tokenHash: device.tokenHash,
                name: device.name,
                platform: device.platform,
                version: device.version,
                machine: device.machine,
                publicKey: device.publicKey,
                accountId: null,
                createdAt: now,
                lastSeenAt: now,
            });
            return 'ok';
        } catch (error) {
            if (!(error instanceof MongoServerError) || error.code !== DUPLICATE_KEY)
                throw error;
        }
        // 已经登记过：令牌对得上才更新，对不上就是别人的
        const result = await this.#devices.updateOne(
            { _id: device.id, tokenHash: device.tokenHash },
            { $set: {
                name: device.name, platform: device.platform, version: device.version, machine: device.machine,
                publicKey: device.publicKey, lastSeenAt: now,
            } },
        );
        return result.matchedCount === 1 ? 'ok' : 'taken';
    }

    async rename(id: string, name: string): Promise<void> {
        await this.#devices.updateOne({ _id: id }, { $set: { name } });
    }

    async bind(id: string, accountId: string | null): Promise<void> {
        await this.#devices.updateOne({ _id: id }, { $set: { accountId } });
    }

    async accountOf(id: string): Promise<string | null> {
        const device = await this.#devices.findOne({ _id: id }, { projection: { accountId: 1 } });
        return device?.accountId ?? null;
    }

    async device(id: string): Promise<DeviceRecord | null> {
        const d = await this.#devices.findOne({ _id: id });
        return d ? { id: d._id, name: d.name, platform: d.platform, publicKey: d.publicKey, machine: d.machine } : null;
    }

    async devicesOf(accountId: string): Promise<DeviceRecord[]> {
        const devices = await this.#devices.find({ accountId }).sort({ createdAt: 1 }).toArray();
        return devices.map((d) => ({
            id: d._id, name: d.name, platform: d.platform, publicKey: d.publicKey, machine: d.machine,
        }));
    }

    async adopt(to: string, from: string): Promise<void> {
        const source = await this.#devices.findOne({ _id: from });
        if (!source)
            return;
        await this.#devices.updateOne({ _id: to }, { $set: {
            tokenHash: source.tokenHash, name: source.name, platform: source.platform, version: source.version,
            machine: source.machine, publicKey: source.publicKey, lastSeenAt: new Date(),
        } });
        await this.#devices.deleteOne({ _id: from });
    }

    async unbindAll(accountId: string): Promise<void> {
        await this.#devices.updateMany({ accountId }, { $set: { accountId: null } });
    }
}
