import { createReadStream } from 'node:fs';

import websocket from '@fastify/websocket';
import Fastify, { type FastifyInstance } from 'fastify';

import { parseFeedback, type FeedbackStore } from './feedback.js';
import { CloseCode, Hub, type HubOptions, type HubSources } from './hub.js';
import { feedbackMail } from './mail.js';
import { hashToken } from './registry.js';
import { fileType, isPlatform, type Releases } from './releases.js';
import { registerWeb, type WebOptions } from './web.js';

export interface ServerOptions extends HubSources {
    // 测试里关掉
    logger: boolean;
    hub?: HubOptions;
    // 每隔多久 ping 一次；上一次的 ping 没等到 pong 就断开（掉线的、睡眠了的电脑）
    heartbeatMs?: number;
    // 程序的新版本（管理后台发布的）；没有时不提供下载
    releases?: Releases;
    // 用户提交的反馈存在这里；adminUrl：通知邮件里的链接（管理后台）。没有时不收反馈
    feedback?: { store: FeedbackStore; adminUrl: string };
    // 网页版的接口（web.ts）；没有时网页版用不了。通行证同时交给 Hub（HubSources.tickets：电脑的回答记在里面）
    web?: WebOptions;
}

// 反馈（feedback.ts）：同一台设备一天最多 5 条，同一个地址 20 条；通知邮件一天最多 20 封（多的在后台里看）
const DAY_S = 24 * 60 * 60;
const FEEDBACK_PER_DEVICE = 5;
const FEEDBACK_PER_ADDRESS = 20;
const FEEDBACK_MAILS_PER_DAY = 20;
// 请求体最大：日志（feedback.ts 的 MAX_LOGS_BYTES）写成 JSON 以后会大一些
const FEEDBACK_BODY_LIMIT = 8 * 1024 * 1024;

// 程序来这里看有没有新版本、下载安装包（官网的下载按钮也问它）
export const UPDATES_PATH = '/updates';

// GET /ws：后台服务的长连接（消息见 protocol.ts）；GET /health：给 Nginx、部署脚本看活着没有；
// GET /updates/...：新版本（releases.ts）；POST /feedback：用户提交反馈（程序的后台服务发，feedback.ts）；
// /web/...：网页版的账号、要连一台电脑（web.ts，docs/web.md）
export async function buildServer(options: ServerOptions): Promise<{ app: FastifyInstance; hub: Hub }> {
    // 在本机的 Nginx 后面：设备的真实地址是 Nginx 加在 X-Forwarded-For 最后的那一个（前面的设备自己能乱写）
    const app = Fastify({ logger: options.logger, trustProxy: ['127.0.0.1', '::1'] });
    const hub = new Hub(options.web ? { ...options, tickets: options.web.tickets } : options, app.log, options.hub);
    app.addHook('onClose', async () => hub.close());
    const heartbeatMs = options.heartbeatMs ?? 30_000;

    // 停止时直接断开连着的设备，不等关闭握手（它们马上重连到另一个实例）：Hub 等它们都下线了才算关好
    await app.register(websocket, {
        options: { maxPayload: 64 * 1024 },
        preClose(done) {
            for (const client of this.websocketServer.clients)
                client.terminate();
            this.websocketServer.close(() => done());
        },
    });

    app.get('/health', async () => ({ ok: true, online: hub.onlineCount }));

    const releases = options.releases;
    if (releases) {
        // 现在的版本：程序比较版本号、核对签名，再按 url 下载
        app.get<{ Params: { platform: string } }>(`${UPDATES_PATH}/:platform/latest`, async (request, reply) => {
            const { platform } = request.params;
            const latest = isPlatform(platform) ? await releases.latest(platform) : null;
            if (!latest)
                return reply.code(404).send({ error: 'notFound' });
            return reply.header('cache-control', 'no-cache')
                .send({ ...latest, url: `${UPDATES_PATH}/${platform}/${latest.version}/${latest.file}` });
        });
        app.get<{ Params: { platform: string; version: string; file: string } }>(
            `${UPDATES_PATH}/:platform/:version/:file`, async (request, reply) => {
                const { platform, version, file } = request.params;
                if (!isPlatform(platform))
                    return reply.code(404).send({ error: 'notFound' });
                const full = await releases.file(platform, version, file);
                if (!full)
                    return reply.code(404).send({ error: 'notFound' });
                return reply.type(fileType(platform)).send(createReadStream(full));
            });
    }

    const feedback = options.feedback;
    if (feedback) {
        // 不用登录：设备 ID 和令牌对得上就收。登录了的，联系方式是账号的邮箱
        app.post('/feedback', { bodyLimit: FEEDBACK_BODY_LIMIT }, async (request, reply) => {
            const body = parseFeedback(request.body);
            if (!body)
                return reply.code(400).send({ error: 'invalid' });
            if (await options.limiter.hit(`feedback:address:${request.ip}`, DAY_S) > FEEDBACK_PER_ADDRESS)
                return reply.code(429).send({ error: 'tooMany' });
            if (!await options.registry.owns(body.deviceId, hashToken(body.token)))
                return reply.code(401).send({ error: 'device' });
            if (await options.limiter.hit(`feedback:device:${body.deviceId}`, DAY_S) > FEEDBACK_PER_DEVICE)
                return reply.code(429).send({ error: 'tooMany' });

            // 上面认过这台设备，记录一定在
            const [device, accountId] = await Promise.all([
                options.registry.device(body.deviceId), options.registry.accountOf(body.deviceId)]);
            const account = accountId ? await options.accounts.get(accountId) : null;
            const stored = await feedback.store.add({
                deviceId: body.deviceId,
                deviceName: device!.name,
                platform: device!.platform,
                version: body.version,
                system: body.system,
                contact: account?.email ?? body.contact,
                signedIn: account !== null,
                text: body.text,
                logs: body.logs,
            });
            request.log.info({ deviceId: body.deviceId, feedback: stored.id }, '收到反馈');

            const to = await feedback.store.notifyAddress();
            if (to && options.mailer && await options.limiter.hit('feedback:mails', DAY_S) <= FEEDBACK_MAILS_PER_DAY) {
                try {
                    await options.mailer.send(feedbackMail(to, stored, feedback.adminUrl));
                    await options.stats.count('mails');
                } catch (error) {
                    request.log.error({ err: error }, '反馈的通知邮件没发出去');
                    await options.stats.mailFailed(error instanceof Error ? error.message : String(error));
                }
            }
            return { ok: true };
        });
    }

    if (options.web)
        registerWeb(app, hub, options, options.web);

    app.get('/ws', { websocket: true }, (socket, request) => {
        const connection = hub.connect({
            send: (text) => socket.send(text),
            close: (code, reason) => socket.close(code, reason),
            host: request.hostname,
            address: request.ip,
        });
        socket.on('message', (data, isBinary) => {
            if (isBinary) {
                connection.reject('invalid', CloseCode.Invalid);
                return;
            }
            connection.receive(data.toString());
        });

        let alive = true;
        socket.on('pong', () => {
            alive = true;
        });
        const heartbeat = setInterval(() => {
            if (!alive) {
                socket.terminate();
                return;
            }
            alive = false;
            socket.ping();
        }, heartbeatMs);

        socket.on('close', () => {
            clearInterval(heartbeat);
            connection.closed();
        });
    });

    return { app, hub };
}
