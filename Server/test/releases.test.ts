import { createHash, generateKeyPairSync, sign, type KeyObject } from 'node:crypto';
import { mkdtemp, readFile, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { Readable } from 'node:stream';

import type { FastifyInstance } from 'fastify';
import { afterEach, beforeEach, describe, expect, it } from 'vitest';

import { defaultHubOptions } from '../src/hub.js';
import { compareVersions, releaseKey, releaseText, Releases, signatureValid, type Platform } from '../src/releases.js';
import { buildServer, UPDATES_PATH } from '../src/server.js';
import { MemoryAccounts, MemoryBroker, MemoryCodes, MemoryLimiter, MemoryRegistry, MemoryStats, MemoryWakes } from './memory.js';

// 和 scripts/sign-release.mjs 一样签
export function signRelease(key: KeyObject, platform: Platform, version: string, bytes: Buffer): string {
    const text = releaseText({ platform, version, size: bytes.length, sha256: createHash('sha256').update(bytes).digest('hex') });
    return sign('sha256', Buffer.from(text), { key, dsaEncoding: 'ieee-p1363' }).toString('base64');
}

describe('新版本', () => {
    const keys = generateKeyPairSync('ec', { namedCurve: 'P-256' });
    let directory: string;
    let releases: Releases;
    let app: FastifyInstance;

    beforeEach(async () => {
        directory = await mkdtemp(path.join(tmpdir(), 'rc-releases-'));
        releases = new Releases(directory, keys.publicKey);
        const broker = new MemoryBroker();
        ({ app } = await buildServer({
            registry: new MemoryRegistry(),
            accounts: new MemoryAccounts(),
            bus: broker.bus(),
            limiter: new MemoryLimiter(),
            codes: new MemoryCodes(),
            wakes: new MemoryWakes(),
            mailer: null,
            stats: new MemoryStats(),
            releases,
            logger: false,
            hub: defaultHubOptions,
        }));
    });

    afterEach(async () => {
        await app.close();
        await rm(directory, { recursive: true, force: true });
    });

    it('版本号按段比数字', () => {
        expect(compareVersions('0.10', '0.9')).toBe(1);
        expect(compareVersions('1.2', '1.2.0')).toBe(0);
        expect(compareVersions('1.2.1', '1.3')).toBe(-1);
        expect(compareVersions('0.1.1-beta', '0.1.0-beta')).toBe(1);
        expect(compareVersions('0.1.0', '0.1.0-beta')).toBe(0);
    });

    it('公钥文件：65 字节的 P-256 公钥，和私钥签的对得上', () => {
        const jwk = keys.publicKey.export({ format: 'jwk' });
        const raw = Buffer.concat([Buffer.from([4]), Buffer.from(jwk.x!, 'base64url'), Buffer.from(jwk.y!, 'base64url')]);
        const key = releaseKey(raw.toString('base64'));
        const bytes = Buffer.from('安装包');
        const text = releaseText({ platform: 'macos-arm64', version: '1.0', size: bytes.length,
                                   sha256: createHash('sha256').update(bytes).digest('hex') });
        expect(signatureValid(key, text, signRelease(keys.privateKey, 'macos-arm64', '1.0', bytes))).toBe(true);
        expect(signatureValid(key, text.replace('1.0', '1.1'), signRelease(keys.privateKey, 'macos-arm64', '1.0', bytes))).toBe(false);
        expect(() => releaseKey(Buffer.alloc(33).toString('base64'))).toThrow();
    });

    it('程序看到现在的版本、按地址下载到一模一样的安装包', async () => {
        expect((await app.inject({ url: `${UPDATES_PATH}/macos-arm64/latest` })).statusCode).toBe(404);
        const bytes = Buffer.from('macOS 的安装包 0.2');
        await releases.publish('macos-arm64', '0.2', signRelease(keys.privateKey, 'macos-arm64', '0.2', bytes), Readable.from([bytes]));

        const latest = (await app.inject({ url: `${UPDATES_PATH}/macos-arm64/latest` })).json<{
            version: string; size: number; sha256: string; signature: string; url: string;
        }>();
        expect(latest).toMatchObject({ version: '0.2', size: bytes.length, url: `${UPDATES_PATH}/macos-arm64/0.2/Bilin-0.2.pkg` });
        const download = await app.inject({ url: latest.url });
        expect(download.rawPayload.equals(bytes)).toBe(true);

        // 别的平台、编出来的路径不给
        expect((await app.inject({ url: `${UPDATES_PATH}/windows-x64/latest` })).statusCode).toBe(404);
        expect((await app.inject({ url: `${UPDATES_PATH}/macos-arm64/0.2/latest.json` })).statusCode).toBe(404);
        expect((await app.inject({ url: `${UPDATES_PATH}/macos-arm64/0.2/..%2F..%2Flatest.json` })).statusCode).toBe(404);
    });

    it('Android 的安装包是 .apk，下载时浏览器认得出', async () => {
        const bytes = Buffer.from('Android 的安装包 0.2');
        await releases.publish('android-arm64', '0.2', signRelease(keys.privateKey, 'android-arm64', '0.2', bytes),
            Readable.from([bytes]));
        const latest = (await app.inject({ url: `${UPDATES_PATH}/android-arm64/latest` })).json<{ url: string }>();
        expect(latest.url).toBe(`${UPDATES_PATH}/android-arm64/0.2/Bilin-0.2.apk`);
        const download = await app.inject({ url: latest.url });
        expect(download.headers['content-type']).toBe('application/vnd.android.package-archive');
        expect(download.rawPayload.equals(bytes)).toBe(true);
    });

    it('签名不对的不收，也不留下上传了一半的文件', async () => {
        const bytes = Buffer.from('安装包');
        await expect(releases.publish('macos-arm64', '0.2', Buffer.alloc(64).toString('base64'), Readable.from([bytes])))
            .rejects.toThrow('signature');
        expect(await releases.list('macos-arm64')).toEqual([]);
        expect(await releases.latest('macos-arm64')).toBeNull();
        const leftovers = await import('node:fs/promises').then((fs) => fs.readdir(path.join(directory, 'macos-arm64')));
        expect(leftovers).toEqual([]);
    });

    it('发布记录：现在的版本写在 latest.json 里', async () => {
        const bytes = Buffer.from('Windows 的安装包');
        await releases.publish('windows-x64', '0.2.1', signRelease(keys.privateKey, 'windows-x64', '0.2.1', bytes), Readable.from([bytes]));
        const latest = JSON.parse(await readFile(path.join(directory, 'windows-x64', 'latest.json'), 'utf8')) as { file: string };
        expect(latest.file).toBe('Bilin-Setup-0.2.1.exe');
    });
});
