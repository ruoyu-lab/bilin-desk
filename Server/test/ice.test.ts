import { mkdtemp, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

import { afterEach, beforeEach, describe, expect, it } from 'vitest';

import { iceServers, loadIceSettings, turnCredentials, type IceSettings } from '../src/ice.js';

const settings: IceSettings = { secret: 'dev-secret', host: null, port: 3478, transport: 'udp', ttlS: 86400 };

describe('turnCredentials', () => {
    // 和 coturn 的算法一样：printf '1760086400:482915307' | openssl dgst -sha1 -hmac dev-secret -binary | base64
    it('signs "expiry:device" with the shared secret', () => {
        expect(turnCredentials('dev-secret', '482915307', 1760086400)).toEqual({
            username: '1760086400:482915307', password: 'hmuod/8n9x/frDtJYM1js4iI45o=',
        });
    });
});

describe('iceServers', () => {
    it('points at the host the device used, with credentials valid for ttl', () => {
        const ice = iceServers(settings, '482915307', '192.168.55.173', 1760000000_500);
        expect(ice).toEqual({
            stun: ['192.168.55.173:3478'],
            turn: {
                server: '192.168.55.173:3478', transport: 'udp',
                ...turnCredentials('dev-secret', '482915307', 1760000000 + 86400),
            },
            ttl: 86400,
        });
    });

    it('uses TURN_HOST when it is set', () => {
        const ice = iceServers({ ...settings, host: 'turn.example.com', transport: 'tcp' }, '482915307', 'localhost', 0);
        expect(ice.stun).toEqual(['turn.example.com:3478']);
        expect(ice.turn.server).toBe('turn.example.com:3478');
        expect(ice.turn.transport).toBe('tcp');
    });
});

describe('loadIceSettings', () => {
    let directory: string;
    let secretFile: string;

    beforeEach(async () => {
        directory = await mkdtemp(join(tmpdir(), 'rc-ice-'));
        secretFile = join(directory, 'turn-secret');
        await writeFile(secretFile, 'dev-secret\n');
    });

    afterEach(async () => {
        await rm(directory, { recursive: true });
    });

    it('is off without a secret file', async () => {
        expect(await loadIceSettings({})).toBeNull();
    });

    it('reads the secret and the defaults', async () => {
        expect(await loadIceSettings({ TURN_SECRET_FILE: secretFile })).toEqual(settings);
        expect(await loadIceSettings({ TURN_SECRET_FILE: secretFile, TURN_HOST: 'turn.example.com', TURN_PORT: '443',
            TURN_TRANSPORT: 'tcp', TURN_TTL_S: '3600' }))
            .toEqual({ secret: 'dev-secret', host: 'turn.example.com', port: 443, transport: 'tcp', ttlS: 3600 });
    });

    it.each([
        ['an empty secret', {}, ' \n'],
        ['a bad port', { TURN_PORT: 'x' }, 'dev-secret'],
        ['an unknown transport', { TURN_TRANSPORT: 'tls' }, 'dev-secret'],
        ['a short ttl', { TURN_TTL_S: '10' }, 'dev-secret'],
        ['a ttl over a week', { TURN_TTL_S: String(8 * 24 * 3600) }, 'dev-secret'],
    ])('refuses %s', async (_, env, secret) => {
        await writeFile(secretFile, secret);
        await expect(loadIceSettings({ TURN_SECRET_FILE: secretFile, ...env })).rejects.toThrow();
    });
});
