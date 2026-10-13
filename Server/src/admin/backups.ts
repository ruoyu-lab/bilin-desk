import { spawn } from 'node:child_process';
import { mkdir, readdir, rename, rm, stat } from 'node:fs/promises';
import path from 'node:path';

// 数据库的备份：mongodump 导出整个库（账号、设备登记），gzip 压缩成一个文件，放在 directory 里，
// 只留最新的 keep 份。名字里是导出的时间（UTC）。恢复见 docs/admin.md
export interface Backup {
    name: string;
    size: number;
    createdAt: string; // ISO 时间
}

// 把库导出到 file（测试里给假的）
export type Dump = (file: string) => Promise<void>;

const NAME = /^bilin-\d{8}-\d{6}\.archive\.gz$/;

export function isBackupName(name: string): boolean {
    return NAME.test(name);
}

function backupName(date: Date): string {
    const stamp = date.toISOString().replace(/[-:]/g, '').replace('T', '-').slice(0, 15);
    return `bilin-${stamp}.archive.gz`;
}

export function mongodump(mongoUrl: string): Dump {
    return (file) =>
        new Promise((resolve, reject) => {
            const child = spawn('mongodump', [`--uri=${mongoUrl}`, `--archive=${file}`, '--gzip', '--quiet'], {
                stdio: ['ignore', 'ignore', 'pipe'],
            });
            let errors = '';
            child.stderr.on('data', (chunk: Buffer) => {
                errors += chunk.toString();
            });
            child.on('error', reject);
            child.on('close', (code) => {
                if (code === 0)
                    resolve();
                else
                    reject(new Error(`mongodump 退出码 ${code}：${errors.trim()}`));
            });
        });
}

export class Backups {
    readonly #directory: string;
    readonly #keep: number;
    readonly #dump: Dump;
    #running: Promise<Backup> | null = null;

    constructor(directory: string, keep: number, dump: Dump) {
        this.#directory = directory;
        this.#keep = keep;
        this.#dump = dump;
    }

    get running(): boolean {
        return this.#running !== null;
    }

    // 新的在前
    async list(): Promise<Backup[]> {
        await mkdir(this.#directory, { recursive: true });
        const names = (await readdir(this.#directory)).filter(isBackupName).sort().reverse();
        return Promise.all(names.map(async (name) => {
            const info = await stat(path.join(this.#directory, name));
            return { name, size: info.size, createdAt: info.mtime.toISOString() };
        }));
    }

    // 正在备份时等那一次，不再开一次
    create(): Promise<Backup> {
        this.#running ??= this.#create().finally(() => {
            this.#running = null;
        });
        return this.#running;
    }

    // 名字不对、不存在时 null
    async file(name: string): Promise<string | null> {
        if (!isBackupName(name))
            return null;
        const file = path.join(this.#directory, name);
        try {
            return (await stat(file)).isFile() ? file : null;
        } catch {
            return null;
        }
    }

    async remove(name: string): Promise<boolean> {
        const file = await this.file(name);
        if (!file)
            return false;
        await rm(file);
        return true;
    }

    async #create(): Promise<Backup> {
        await mkdir(this.#directory, { recursive: true });
        const name = backupName(new Date());
        const file = path.join(this.#directory, name);
        // 导出完才换成正式的名字：没导完的不会出现在列表里
        const partial = `${file}.partial`;
        try {
            await this.#dump(partial);
            await rename(partial, file);
        } catch (error) {
            await rm(partial, { force: true });
            throw error;
        }
        await this.#prune();
        const info = await stat(file);
        return { name, size: info.size, createdAt: info.mtime.toISOString() };
    }

    async #prune(): Promise<void> {
        const old = (await this.list()).slice(this.#keep);
        await Promise.all(old.map((backup) => rm(path.join(this.#directory, backup.name))));
    }
}

// 每天 hour 点（服务器的本地时间）备份一次
export function scheduleDaily(hour: number, run: () => Promise<void>): () => void {
    let timer: NodeJS.Timeout;
    const next = () => {
        const now = new Date();
        const at = new Date(now);
        at.setHours(hour, 0, 0, 0);
        if (at <= now)
            at.setDate(at.getDate() + 1);
        timer = setTimeout(() => void run().finally(next), at.getTime() - now.getTime());
    };
    next();
    return () => clearTimeout(timer);
}
