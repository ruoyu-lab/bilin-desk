import { createReadStream } from 'node:fs';

import websocket from '@fastify/websocket';
import Fastify, { type FastifyInstance } from 'fastify';

import { CloseCode, Hub, type HubOptions, type HubSources } from './hub.js';
import { fileType, isPlatform, type Releases } from './releases.js';

export interface ServerOptions extends HubSources {
    // 测试里关掉
    logger: boolean;
    hub?: HubOptions;
    // 每隔多久 ping 一次；上一次的 ping 没等到 pong 就断开（掉线的、睡眠了的电脑）
    heartbeatMs?: number;
    // 程序的新版本（管理后台发布的）；没有时不提供下载
    releases?: Releases;
}

// 程序来这里看有没有新版本、下载安装包（官网的下载按钮也问它）
export const UPDATES_PATH = '/updates';

// GET /ws：后台服务的长连接（消息见 protocol.ts）；GET /health：给 Nginx、部署脚本看活着没有；
// GET /updates/...：新版本（releases.ts）
export async function buildServer(options: ServerOptions): Promise<{ app: FastifyInstance; hub: Hub }> {
    // 在本机的 Nginx 后面：设备的真实地址是 Nginx 加在 X-Forwarded-For 最后的那一个（前面的设备自己能乱写）
    const app = Fastify({ logger: options.logger, trustProxy: ['127.0.0.1', '::1'] });
    const hub = new Hub(options, app.log, options.hub);
    app.addHook('onClose', async () => hub.close());
    const heartbeatMs = options.heartbeatMs ?? 30_000;

    await app.register(websocket, { options: { maxPayload: 64 * 1024 } });

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
