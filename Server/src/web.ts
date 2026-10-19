import { randomBytes } from 'node:crypto';

import type { FastifyInstance, FastifyReply, FastifyRequest } from 'fastify';

import type { Account } from './accounts.js';
import type { Hub, HubSources } from './hub.js';
import { DEVICE_ID, parseClientMessage, type AccountFailure } from './protocol.js';
import type { RelayStatus } from './relay-load.js';
import type { Tickets } from './tickets.js';
import { WEB_SESSION_TTL_S, type WebSessions } from './web-sessions.js';

// 网页版的接口（docs/web.md），都在 /web/ 下（web.bilindesk.com 的 Nginx 转过来）：
// - 账号：POST login、logout、code（发验证码）、register、reset（找回密码）；GET me（登录着的账号和它下面的设备）。
//   登录以后是一个 HttpOnly、SameSite=Strict 的 cookie（web-sessions.ts）；改东西的请求还核对 Origin
// - POST connect：要连一台电脑。服务端给那台电脑、这个浏览器各发一张进它房间的一次性通行证（tickets.ts），经长连接告诉电脑，
//   等电脑回答准备好了（进了转发进程里的房间）才把浏览器的那张交出去。浏览器登录了那台电脑的账号时（我的设备）还给一把
//   这次连接用的一次性钥匙，不用临时密码、不用点允许；别人的电脑要输临时密码、对方点允许
export interface WebOptions {
    tickets: Tickets;
    sessions: WebSessions;
    relay: RelayStatus;
    // 等电脑回答多久、隔多久看一次
    answerWaitMs?: number;
    answerPollMs?: number;
}

const COOKIE = 'bilin_web';
// 同一个地址一小时最多要连 60 次
const CONNECTS_PER_ADDRESS = 60;
const HOUR_S = 3600;

const failureStatus: Record<AccountFailure, number> = {
    emailTaken: 409, noAccount: 404, wrongPassword: 401, wrongCode: 401, tooMany: 429, mailLimit: 503,
    mailFailed: 502,
};

function cookieValue(request: FastifyRequest): string | null {
    for (const part of (request.headers.cookie ?? '').split(';')) {
        const [key, ...value] = part.trim().split('=');
        if (key === COOKIE)
            return value.join('=');
    }
    return null;
}

function setCookie(reply: FastifyReply, token: string, maxAgeS: number): FastifyReply {
    // localhost 上的 http 页面也收 Secure 的 cookie（本机开发）
    return reply.header('set-cookie', `${COOKIE}=${token}; Path=/web; Max-Age=${maxAgeS}; HttpOnly; Secure; SameSite=Strict`);
}

export function registerWeb(app: FastifyInstance, hub: Hub, sources: HubSources, options: WebOptions): void {
    const { tickets, sessions, relay } = options;
    const answerWaitMs = options.answerWaitMs ?? 8000;
    const answerPollMs = options.answerPollMs ?? 200;

    app.register(async (web) => {
        // 改东西的请求只认网页版自己发的：cookie 是 SameSite=Strict，再看一下 Origin
        web.addHook('onRequest', async (request, reply) => {
            if (request.method === 'GET' || request.method === 'HEAD')
                return;
            const origin = request.headers.origin;
            if (origin && new URL(origin).host !== request.headers.host)
                return reply.code(403).send({ error: 'origin' });
        });

        const accountOf = async (request: FastifyRequest): Promise<string | null> => {
            const token = cookieValue(request);
            return token ? sessions.account(token) : null;
        };

        // 账号和它下面的设备（不带公钥：浏览器用不上）
        const view = async (accountId: string | null) => {
            const current = accountId ? await hub.webView(accountId) : null;
            return {
                account: current?.account ?? null,
                devices: (current?.devices ?? []).map(({ deviceId, name, platform, online }) => ({ deviceId, name, platform, online })),
            };
        };

        const signIn = async (reply: FastifyReply, result: Account | AccountFailure) => {
            if (typeof result === 'string')
                return reply.code(failureStatus[result]).send({ error: result });
            const token = await sessions.create(result.id);
            return setCookie(reply, token, WEB_SESSION_TTL_S).send(await view(result.id));
        };

        // 请求体照客户端的消息核对（protocol.ts），规则一样
        const parse = (type: string, body: unknown) =>
            parseClientMessage(JSON.stringify({ ...(typeof body === 'object' && body ? body : {}), type }));

        web.get('/me', async (request) => view(await accountOf(request)));

        web.post('/login', async (request, reply) => {
            const message = parse('login', request.body);
            if (message?.type !== 'login')
                return reply.code(400).send({ error: 'invalid' });
            return signIn(reply, await hub.webSignIn(request.ip, message));
        });

        web.post('/logout', async (request, reply) => {
            const token = cookieValue(request);
            if (token)
                await sessions.remove(token);
            return setCookie(reply, '', 0).send({ ok: true });
        });

        web.post('/code', async (request, reply) => {
            const message = parse('sendCode', request.body);
            if (message?.type !== 'sendCode')
                return reply.code(400).send({ error: 'invalid' });
            const failure = await hub.sendCode('web', request.ip, message);
            return failure ? reply.code(failureStatus[failure]).send({ error: failure }) : { ok: true };
        });

        web.post('/register', async (request, reply) => {
            const message = parse('register', request.body);
            if (message?.type !== 'register')
                return reply.code(400).send({ error: 'invalid' });
            const result = await hub.createAccount(message);
            if (typeof result !== 'string')
                request.log.info({ accountId: result.id }, '网页版注册了账号');
            return signIn(reply, result);
        });

        web.post('/reset', async (request, reply) => {
            const message = parse('resetPassword', request.body);
            if (message?.type !== 'resetPassword')
                return reply.code(400).send({ error: 'invalid' });
            return signIn(reply, await hub.webResetPassword(message));
        });

        web.post<{ Body: { deviceId?: unknown } }>('/connect', async (request, reply) => {
            const deviceId = request.body?.deviceId;
            if (typeof deviceId !== 'string' || !DEVICE_ID.test(deviceId))
                return reply.code(400).send({ error: 'invalid' });
            if (await sources.limiter.hit(`web:address:${request.ip}`, HOUR_S) > CONNECTS_PER_ADDRESS)
                return reply.code(429).send({ error: 'tooMany' });
            const load = await relay.load();
            if (!load)
                return reply.code(503).send({ error: 'unavailable' });
            if (load.viewers >= load.max)
                return reply.code(503).send({ error: 'busy' });

            const accountId = await accountOf(request);
            const mine = accountId !== null && await sources.registry.accountOf(deviceId) === accountId;
            const room = mine ? `${deviceId}/mine` : deviceId;
            const key = mine ? randomBytes(32).toString('base64') : undefined;
            const share = await tickets.issue({ room, role: 'share' });
            if (!await hub.webViewer(deviceId, share, key))
                return reply.code(404).send({ error: 'offline' });

            // 等电脑准备好：进了房间（浏览器的握手才有人接）、换好了钥匙
            let answer: string | null = null;
            for (let waited = 0; answer === null && waited < answerWaitMs; waited += answerPollMs) {
                await new Promise((resolve) => setTimeout(resolve, answerPollMs));
                answer = await tickets.answered(share);
            }
            if (answer === null)
                return reply.code(504).send({ error: 'noAnswer' });
            if (answer !== '')
                return reply.code(409).send({ error: 'declined', reason: answer });
            return { ticket: await tickets.issue({ room, role: 'view' }), ...(key ? { key } : {}) };
        });
    }, { prefix: '/web' });
}
