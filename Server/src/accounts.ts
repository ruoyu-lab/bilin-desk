import { randomBytes, randomUUID, scrypt, timingSafeEqual } from 'node:crypto';
import { promisify } from 'node:util';

import { type Collection, MongoServerError } from 'mongodb';

const scryptAsync = promisify(scrypt) as (password: string, salt: Buffer, length: number) => Promise<Buffer>;

export interface Account {
    id: string;
    email: string;
    nickname: string;
}

// 账号：邮箱 + 密码（注册时验证过邮箱：hub.ts、codes.ts）。邮箱不分大小写，密码只存 scrypt 的结果
export interface Accounts {
    // 邮箱已经注册过：emailTaken
    create(email: string, nickname: string, password: string): Promise<Account | 'emailTaken'>;
    // 没有这个邮箱、密码不对：null（不说是哪一种）
    verify(email: string, password: string): Promise<Account | null>;
    get(id: string): Promise<Account | null>;
    // 用这个邮箱注册的账号
    byEmail(email: string): Promise<Account | null>;
    // 账号 id 的密码是不是它
    checkPassword(id: string, password: string): Promise<boolean>;
    setNickname(id: string, nickname: string): Promise<void>;
    setPassword(id: string, password: string): Promise<void>;
    remove(id: string): Promise<void>;
}

const KEY_LENGTH = 64;

// "scrypt$盐$结果"，都是 base64
export async function hashPassword(password: string): Promise<string> {
    const salt = randomBytes(16);
    const hash = await scryptAsync(password, salt, KEY_LENGTH);
    return `scrypt$${salt.toString('base64')}$${hash.toString('base64')}`;
}

export async function checkPassword(password: string, stored: string): Promise<boolean> {
    const [kind, salt, hash] = stored.split('$');
    if (kind !== 'scrypt' || !salt || !hash)
        return false;
    const expected = Buffer.from(hash, 'base64');
    const actual = await scryptAsync(password, Buffer.from(salt, 'base64'), expected.length);
    return timingSafeEqual(actual, expected);
}

export interface AccountDocument {
    _id: string;
    email: string;
    nickname: string;
    passwordHash: string;
    createdAt: Date;
}

const DUPLICATE_KEY = 11000;

export class MongoAccounts implements Accounts {
    readonly #accounts: Collection<AccountDocument>;

    constructor(accounts: Collection<AccountDocument>) {
        this.#accounts = accounts;
    }

    // 启动时建好：同一个邮箱只能注册一次
    async prepare(): Promise<void> {
        await this.#accounts.createIndex({ email: 1 }, { unique: true });
    }

    async create(email: string, nickname: string, password: string): Promise<Account | 'emailTaken'> {
        const account: AccountDocument = {
            _id: randomUUID(), email, nickname, passwordHash: await hashPassword(password), createdAt: new Date(),
        };
        try {
            await this.#accounts.insertOne(account);
        } catch (error) {
            if (error instanceof MongoServerError && error.code === DUPLICATE_KEY)
                return 'emailTaken';
            throw error;
        }
        return { id: account._id, email, nickname };
    }

    async verify(email: string, password: string): Promise<Account | null> {
        const account = await this.#accounts.findOne({ email });
        if (!account || !await checkPassword(password, account.passwordHash))
            return null;
        return { id: account._id, email: account.email, nickname: account.nickname };
    }

    async get(id: string): Promise<Account | null> {
        const account = await this.#accounts.findOne({ _id: id });
        return account ? { id: account._id, email: account.email, nickname: account.nickname } : null;
    }

    async byEmail(email: string): Promise<Account | null> {
        const account = await this.#accounts.findOne({ email });
        return account ? { id: account._id, email: account.email, nickname: account.nickname } : null;
    }

    async checkPassword(id: string, password: string): Promise<boolean> {
        const account = await this.#accounts.findOne({ _id: id }, { projection: { passwordHash: 1 } });
        return account !== null && await checkPassword(password, account.passwordHash);
    }

    async setNickname(id: string, nickname: string): Promise<void> {
        await this.#accounts.updateOne({ _id: id }, { $set: { nickname } });
    }

    async setPassword(id: string, password: string): Promise<void> {
        await this.#accounts.updateOne({ _id: id }, { $set: { passwordHash: await hashPassword(password) } });
    }

    async remove(id: string): Promise<void> {
        await this.#accounts.deleteOne({ _id: id });
    }
}
