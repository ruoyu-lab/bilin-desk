import { readFile } from 'node:fs/promises';
import { homedir } from 'node:os';
import path from 'node:path';

import { Redis } from 'ioredis';
import { MongoClient } from 'mongodb';

import { MongoFeedback, type FeedbackDocument, type SettingsDocument } from '../feedback.js';
import { defaultHubOptions } from '../hub.js';
import { RedisLimiter } from '../limiter.js';
import { Releases, releaseKey } from '../releases.js';
import { loadMailer, testMail } from '../mail.js';
import { recordOnline, RedisStats } from '../stats.js';
import { Backups, mongodump, scheduleDaily } from './backups.js';
import { clearLogs, recentProblems } from './logs.js';
import { buildAdmin } from './server.js';
import { RedisSessions } from './sessions.js';
import { onlineDevices, systemStatus, type MailMode } from './status.js';

// 管理后台（docs/admin.md）：单独一个进程（PM2 的 bilin-admin），只听本机，Nginx 把 base 下的请求转过来。
// 发布的新版本放在 DATA_DIR/updates，服务端（main.ts）从同一个地方给程序下载
// 配置都从环境变量来（ecosystem.config.cjs）
const host = process.env.HOST ?? '127.0.0.1';
const port = Number(process.env.PORT ?? 3100);
const base = process.env.BASE_PATH ?? '';
const mongoUrl = process.env.MONGO_URL ?? 'mongodb://127.0.0.1:27017/bilin';
const redisUrl = process.env.REDIS_URL ?? 'redis://127.0.0.1:6379';
const dataDirectory = process.env.DATA_DIR ?? '/var/lib/bilin';
const passwordFile = process.env.ADMIN_PASSWORD_FILE ?? '/etc/bilin/admin-password';
const healthUrl = process.env.HEALTH_URL ?? 'http://127.0.0.1:3000/health';
const backupHour = Number(process.env.BACKUP_HOUR ?? 4);
const backupKeep = Number(process.env.BACKUP_KEEP ?? 14);
const secureCookie = process.env.COOKIE_SECURE !== 'false';
// 看证书的域名：开发用的虚拟机没有（空的）
const certificateHost = process.env.CERT_HOST || null;
// PM2 的日志在它所在用户的家目录下
const logDirectory = process.env.LOG_DIR ?? path.join(homedir(), '.pm2', 'logs');

// 验证码邮件现在怎么发：和服务端读同样的设置（ecosystem.config.cjs）；密钥可能是刚设上的，每次都看
async function mailMode(): Promise<MailMode> {
    if (process.env.MAIL === 'log')
        return 'log';
    if (!process.env.MAIL_KEY_FILE)
        return 'none';
    const key = await readFile(process.env.MAIL_KEY_FILE, 'utf8').catch(() => '');
    return key.trim() ? 'resend' : 'none';
}

const mongo = new MongoClient(mongoUrl);
await mongo.connect();
const redis = new Redis(redisUrl);
const sessions = new RedisSessions(redisUrl);
const limiter = new RedisLimiter(redisUrl);
const stats = new RedisStats(redisUrl);
const feedback = new MongoFeedback(mongo.db().collection<FeedbackDocument>('feedback'),
                                 mongo.db().collection<SettingsDocument>('settings'));
const backups = new Backups(path.join(dataDirectory, 'backups'), backupKeep, mongodump(mongoUrl));
const releases = new Releases(path.join(dataDirectory, 'updates'),
                              releaseKey(await readFile(new URL('../../release-key.pub', import.meta.url), 'utf8')));

const app = await buildAdmin({
    base,
    passwordFile,
    sessions,
    limiter,
    backups,
    releases,
    feedback,
    status: systemStatus({
        db: mongo.db(), redis, dataDirectory, healthUrl, certificateHost, mailMode,
        mailLimit: defaultHubOptions.limits.mailsPerDay,
        latestBackup: async () => (await backups.list())[0] ?? null,
    }),
    logs: () => recentProblems(logDirectory),
    clearLogs: () => clearLogs(logDirectory),
    // 每次都重新读密钥（可能刚换过）；发出去的、没发出去的和验证码邮件记在一起（都占 Resend 的额度）
    testMail: async (to) => {
        const mailer = await loadMailer(process.env, app.log);
        if (!mailer)
            throw new Error('还没配 Resend 的密钥（Server/deploy/set-mail-key.sh）');
        try {
            await mailer.send(testMail(to));
        } catch (error) {
            await stats.mailFailed(error instanceof Error ? error.message : String(error));
            throw error;
        }
        await stats.count('mails');
    },
    secureCookie,
    logger: true,
});
await app.listen({ host, port });

// 每天备份一次（只留最新的 backupKeep 份）
const stopSchedule = scheduleDaily(backupHour, async () => {
    try {
        const backup = await backups.create();
        app.log.info({ name: backup.name, size: backup.size }, '每天的备份');
    } catch (error) {
        app.log.error({ err: error }, '每天的备份没成');
    }
});

// 每分钟记一次在线的设备有几台：每天最多的那个数（"最近 7 天"）
const onlineSampler = setInterval(() => {
    onlineDevices(redis).then((online) => recordOnline(redis, online))
        .catch((error: unknown) => app.log.error({ err: error }, '记不了在线的设备数'));
}, 60_000);

let stopping = false;
async function stop(signal: string): Promise<void> {
    if (stopping)
        return;
    stopping = true;
    app.log.info({ signal }, '停止');
    stopSchedule();
    clearInterval(onlineSampler);
    await app.close();
    await Promise.all([sessions.close(), limiter.close(), stats.close(), redis.quit(), mongo.close()]);
    process.exit(0);
}
process.on('SIGINT', (signal) => void stop(signal));
process.on('SIGTERM', (signal) => void stop(signal));
