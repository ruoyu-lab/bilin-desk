import { open, readdir, truncate, unlink } from 'node:fs/promises';
import path from 'node:path';

// 服务端、管理后台最近的警告和出错（管理后台的"出错日志"）：读 PM2 的日志文件。Fastify 的日志一行一个 JSON
// （level 40 是警告、50 以上是出错），进程自己崩了的话 Node 往标准错误写的不是 JSON，也算出错。
// 每个文件只从末尾往前读一段：日志每天轮换，留 14 天
export interface LogEntry {
    time: string; // ISO 时间；读不出时间的为空
    level: 'warn' | 'error';
    app: string; // PM2 里的名字：bilin-server、bilin-admin
    message: string;
    // 出错的具体原因（err.message）
    detail: string;
}

const TAIL_BYTES = 256 * 1024;

// PM2 的日志文件：<名字>-out-<实例>.log、<名字>-error-<实例>.log（单实例的没有 -<实例>）
const FILE = /^(.+)-(out|error)(-\d+)?\.log$/;
// logrotate 每天轮换下来的旧日志：后面接着日期或序号，压缩过（.gz）
const ROTATED = /^(.+)-(out|error)(-\d+)?\.log[.-].+$/;

async function tail(file: string): Promise<string[]> {
    const handle = await open(file, 'r');
    try {
        const { size } = await handle.stat();
        const length = Math.min(size, TAIL_BYTES);
        const buffer = Buffer.alloc(length);
        await handle.read(buffer, 0, length, size - length);
        const lines = buffer.toString('utf8').split('\n');
        // 从中间开始读的：第一行多半不完整
        return length < size ? lines.slice(1) : lines;
    } finally {
        await handle.close();
    }
}

function parse(line: string, app: string, stream: string): LogEntry | null {
    const text = line.trim();
    if (!text)
        return null;
    if (!text.startsWith('{'))
        return stream === 'error' ? { time: '', level: 'error', app, message: text.slice(0, 500), detail: '' } : null;
    let entry: { level?: number; time?: number; msg?: string; err?: { message?: string } };
    try {
        entry = JSON.parse(text) as typeof entry;
    } catch {
        return null;
    }
    if (typeof entry.level !== 'number' || entry.level < 40)
        return null;
    return {
        time: typeof entry.time === 'number' ? new Date(entry.time).toISOString() : '',
        level: entry.level >= 50 ? 'error' : 'warn',
        app,
        message: String(entry.msg ?? ''),
        detail: String(entry.err?.message ?? ''),
    };
}

// directory 里各个日志文件里最近的 limit 条，新的在前（没有时间的排在最后）
export async function recentProblems(directory: string, limit = 100): Promise<LogEntry[]> {
    let names: string[];
    try {
        names = await readdir(directory);
    } catch {
        return [];
    }
    const entries: LogEntry[] = [];
    for (const name of names) {
        const match = FILE.exec(name);
        if (!match)
            continue;
        for (const line of await tail(path.join(directory, name))) {
            const entry = parse(line, match[1]!, match[2]!);
            if (entry)
                entries.push(entry);
        }
    }
    return entries.sort((a, b) => b.time.localeCompare(a.time)).slice(0, limit);
}

// 清空日志（管理后台的"出错日志"里点了清空）：正在写的日志文件清成空的（PM2 开着它们、接着往里追加），
// 轮换下来的旧日志删掉。删了就没了，找不回来。返回清了几个文件
export async function clearLogs(directory: string): Promise<number> {
    let cleared = 0;
    for (const name of await readdir(directory)) {
        const file = path.join(directory, name);
        if (FILE.test(name)) {
            await truncate(file, 0);
            ++cleared;
        } else if (ROTATED.test(name)) {
            await unlink(file);
            ++cleared;
        }
    }
    return cleared;
}
