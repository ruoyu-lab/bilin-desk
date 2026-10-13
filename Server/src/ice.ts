import { createHmac } from 'node:crypto';
import { readFile } from 'node:fs/promises';

import type { IceServers, TurnTransport } from './protocol.js';

// P2P 和中继用的服务器：coturn，同时是 STUN 和 TURN。中继的凭据是 coturn 的 REST 凭据（use-auth-secret）：
// username = 过期时间（秒）:设备 ID，password = base64(HMAC-SHA1(共享密钥, username))。
// coturn 用同一个密钥验证，服务器不用记发出去的凭据；共享密钥只在服务器上（TURN_SECRET_FILE）
export interface IceSettings {
    secret: string;
    // 设备连 coturn 用的主机名；null：用设备连服务器时用的那个（Host 头）。
    // 开发时 coturn 和服务器在同一台虚拟机上，Mac 连的是 localhost、局域网里的电脑连的是 Mac 的地址，跟着 Host 走就对了
    host: string | null;
    port: number;
    transport: TurnTransport;
    ttlS: number;
}

export function turnCredentials(secret: string, deviceId: string, expiresS: number): { username: string; password: string } {
    const username = `${expiresS}:${deviceId}`;
    return { username, password: createHmac('sha1', secret).update(username).digest('base64') };
}

// 给设备 deviceId 的；requestHost：它连服务器时用的主机名
export function iceServers(settings: IceSettings, deviceId: string, requestHost: string, nowMs: number): IceServers {
    const server = `${settings.host ?? requestHost}:${settings.port}`;
    const credentials = turnCredentials(settings.secret, deviceId, Math.floor(nowMs / 1000) + settings.ttlS);
    return { stun: [server], turn: { server, transport: settings.transport, ...credentials }, ttl: settings.ttlS };
}

const MAX_TTL_S = 7 * 24 * 60 * 60;

// 环境变量（PM2 的 ecosystem.config.cjs）：TURN_SECRET_FILE 没设时不发（设备只能直连）。
// TURN_HOST（默认跟着 Host 头）、TURN_PORT（3478）、TURN_TRANSPORT（udp | tcp）、TURN_TTL_S（凭据的时效，默认一天，最多七天）
export async function loadIceSettings(env: NodeJS.ProcessEnv): Promise<IceSettings | null> {
    const file = env.TURN_SECRET_FILE;
    if (!file)
        return null;
    const secret = (await readFile(file, 'utf8')).trim();
    const port = Number(env.TURN_PORT ?? 3478);
    const transport = env.TURN_TRANSPORT ?? 'udp';
    const ttlS = Number(env.TURN_TTL_S ?? 24 * 60 * 60);
    if (!secret)
        throw new Error(`${file} 是空的`);
    if (!Number.isInteger(port) || port < 1 || port > 65535)
        throw new Error(`TURN_PORT 不对：${env.TURN_PORT}`);
    if (transport !== 'udp' && transport !== 'tcp')
        throw new Error(`TURN_TRANSPORT 只能是 udp 或 tcp：${transport}`);
    if (!Number.isInteger(ttlS) || ttlS < 60 || ttlS > MAX_TTL_S)
        throw new Error(`TURN_TTL_S 要在 60 秒到七天之间：${env.TURN_TTL_S}`);
    return { secret, host: env.TURN_HOST || null, port, transport, ttlS };
}
