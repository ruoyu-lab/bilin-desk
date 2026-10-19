import { RedisRelayStatus } from '../relay-load.js';
import { RedisTickets } from '../tickets.js';
import { buildRelay } from './server.js';

// 网页版的转发进程（docs/web.md）：单独一个进程（PM2 的 bilin-relay），只听本机，Nginx 把 /relay 转过来。
// 浏览器和被连的电脑必须进同一个进程才配得上对，所以只开一个。配置从环境变量来（ecosystem.config.cjs）
const host = process.env.HOST ?? '127.0.0.1';
const port = Number(process.env.PORT ?? 3200);
const redisUrl = process.env.REDIS_URL ?? 'redis://127.0.0.1:6379';
// 网页版最多几路、发给一个浏览器每秒最多多少字节（服务器的带宽是共用的，docs/web.md）
const maxViewers = Number(process.env.WEB_MAX_SESSIONS ?? 20);
const viewerBytesPerSecond = Number(process.env.WEB_MAX_VIEWER_BPS ?? 8_000_000) / 8;

const tickets = new RedisTickets(redisUrl);
const status = new RedisRelayStatus(redisUrl);
const app = await buildRelay({ tickets, status, logger: true, maxViewers, viewerBytesPerSecond });
await app.listen({ host, port });

let stopping = false;
async function stop(signal: string): Promise<void> {
    if (stopping)
        return;
    stopping = true;
    app.log.info({ signal }, '停止');
    await app.close();
    await Promise.all([tickets.close(), status.close()]);
    process.exit(0);
}
process.on('SIGINT', (signal) => void stop(signal));
process.on('SIGTERM', (signal) => void stop(signal));
