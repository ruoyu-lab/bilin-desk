import { readFile } from 'node:fs/promises';

import { MongoClient } from 'mongodb';

import { MongoAccounts, type AccountDocument } from './accounts.js';
import { RedisBus } from './bus.js';
import { RedisCodes } from './codes.js';
import { MongoFeedback, type FeedbackDocument, type SettingsDocument } from './feedback.js';
import { defaultHubOptions } from './hub.js';
import { loadIceSettings } from './ice.js';
import { RedisLimiter } from './limiter.js';
import { loadMailer } from './mail.js';
import { RedisStats } from './stats.js';
import { MongoRegistry, type DeviceDocument } from './registry.js';
import { Releases, releaseKey } from './releases.js';
import { buildServer } from './server.js';
import { RedisRelayStatus } from './relay-load.js';
import { RedisTickets } from './tickets.js';
import { RedisWakes } from './wakes.js';
import { RedisWebSessions } from './web-sessions.js';

// 配置都从环境变量来（PM2 的 ecosystem.config.cjs 里设）
const host = process.env.HOST ?? '127.0.0.1';
const port = Number(process.env.PORT ?? 3000);
const mongoUrl = process.env.MONGO_URL ?? 'mongodb://127.0.0.1:27017/bilin';
const redisUrl = process.env.REDIS_URL ?? 'redis://127.0.0.1:6379';
const ice = await loadIceSettings(process.env);
const updatesDirectory = process.env.UPDATES_DIR ?? '/var/lib/bilin/updates';
const releaseKeyFile = new URL('../release-key.pub', import.meta.url);
// 反馈的通知邮件里，管理后台的地址
const adminUrl = process.env.ADMIN_URL ?? 'https://admin.bilindesk.com/';

const mongo = new MongoClient(mongoUrl);
await mongo.connect();
const registry = new MongoRegistry(mongo.db().collection<DeviceDocument>('devices'));
const accounts = new MongoAccounts(mongo.db().collection<AccountDocument>('accounts'));
const feedback = new MongoFeedback(mongo.db().collection<FeedbackDocument>('feedback'),
                                 mongo.db().collection<SettingsDocument>('settings'));
await Promise.all([registry.prepare(), accounts.prepare(), feedback.prepare()]);
const bus = new RedisBus(redisUrl);
const limiter = new RedisLimiter(redisUrl);
const codes = new RedisCodes(redisUrl);
const stats = new RedisStats(redisUrl);
const tickets = new RedisTickets(redisUrl);
const webSessions = new RedisWebSessions(redisUrl);
const relayStatus = new RedisRelayStatus(redisUrl);
const wakes = new RedisWakes(redisUrl);
const releases = new Releases(updatesDirectory, releaseKey(await readFile(releaseKeyFile, 'utf8')));

// 发邮件的日志在 Fastify 建好以前还没有：先交给一个转发的，建好以后接上
const mailLog = { info: (object: object, message: string) => app.log.info(object, message) };
const mailer = await loadMailer(process.env, mailLog);
const { app } = await buildServer({
    registry, accounts, bus, limiter, codes, mailer, stats, wakes, releases, logger: true, hub: { ...defaultHubOptions, ice },
    feedback: { store: feedback, adminUrl: `${adminUrl}#feedback` },
    web: { tickets, sessions: webSessions, relay: relayStatus },
});
await app.listen({ host, port });
if (ice)
    app.log.info({ host: ice.host ?? '跟着 Host 头', port: ice.port, transport: ice.transport }, '中继：coturn');
else
    app.log.warn('没配中继（TURN_SECRET_FILE）：设备之间直连打不通时连不上');
if (!mailer)
    app.log.warn('没配发邮件（MAIL_KEY_FILE）：发不了验证码，注册、找回密码都做不了');

// PM2 平滑重启、停止时：不再接新连接，关掉现有的（客户端会连到另一个实例），再断开数据库
let stopping = false;
async function stop(signal: string): Promise<void> {
    if (stopping)
        return;
    stopping = true;
    app.log.info({ signal }, '停止');
    await app.close();
    await bus.close();
    await limiter.close();
    await codes.close();
    await stats.close();
    await Promise.all([tickets.close(), webSessions.close(), relayStatus.close(), wakes.close()]);
    await mongo.close();
    process.exit(0);
}
process.on('SIGINT', (signal) => void stop(signal));
process.on('SIGTERM', (signal) => void stop(signal));
