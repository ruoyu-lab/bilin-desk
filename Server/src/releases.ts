import { createHash, createPublicKey, randomBytes, verify, type KeyObject } from 'node:crypto';
import { createWriteStream } from 'node:fs';
import { mkdir, readFile, readdir, rename, rm, stat, writeFile } from 'node:fs/promises';
import path from 'node:path';
import type { Readable } from 'node:stream';
import { pipeline } from 'node:stream/promises';

// 程序的新版本（docs/admin.md 的"发布新版本"）。安装包在作者的电脑上用发布密钥签名（scripts/sign-release.mjs），
// 签的是下面 releaseText 那几行：哪个平台、哪个版本、多大、sha256。程序里带着公钥（Server/release-key.pub），
// 下载以后自己核对，服务器被人控制了也推不了假的安装包。服务器上传时也核对一遍，签名不对的不收
export const PLATFORMS = ['macos-arm64', 'windows-x64', 'android-arm64'] as const;
export type Platform = (typeof PLATFORMS)[number];

export interface Release {
    platform: Platform;
    version: string;
    size: number;
    sha256: string; // 十六进制
    signature: string; // base64，P-256 ECDSA 的 r||s（64 字节）
    file: string; // 安装包的文件名
    publishedAt: string; // ISO 时间
}

export type PublishError = 'platform' | 'version' | 'signature' | 'notNewer' | 'empty';

export class ReleaseError extends Error {
    constructor(readonly code: PublishError) {
        super(code);
    }
}

const FILES: Record<Platform, (version: string) => string> = {
    'macos-arm64': (version) => `Bilin-${version}.pkg`,
    'windows-x64': (version) => `Bilin-Setup-${version}.exe`,
    'android-arm64': (version) => `Bilin-${version}.apk`,
};

// 下载时的类型：Android 的浏览器按它认出是安装包，下完点开就能装
export function fileType(platform: Platform): string {
    return platform === 'android-arm64' ? 'application/vnd.android.package-archive' : 'application/octet-stream';
}

export function isPlatform(value: string): value is Platform {
    return (PLATFORMS as readonly string[]).includes(value);
}

// 2 到 4 段数字（CMake 的 project VERSION），可以带着阶段（0.1.0-beta）
export function isVersion(value: string): boolean {
    return /^\d{1,5}(\.\d{1,5}){1,3}(-[a-z]{1,16})?$/.test(value);
}

// 按段比数字，少的段算 0：1.2 和 1.2.0 一样。阶段不算（每次发布数字都变大）
export function compareVersions(a: string, b: string): number {
    const x = a.split('-')[0]!.split('.').map(Number);
    const y = b.split('-')[0]!.split('.').map(Number);
    for (let i = 0; i < Math.max(x.length, y.length); i++) {
        const difference = (x[i] ?? 0) - (y[i] ?? 0);
        if (difference !== 0)
            return Math.sign(difference);
    }
    return 0;
}

// 签名签的内容：程序里（Core/src/rc/release.cpp）一字不差地拼出同样的几行
export function releaseText(release: Pick<Release, 'platform' | 'version' | 'size' | 'sha256'>): string {
    return `bilin-release\nplatform=${release.platform}\nversion=${release.version}\nsize=${release.size}\nsha256=${release.sha256}\n`;
}

// 公钥文件里是 base64 的 P-256 公钥（未压缩的点，65 字节）
export function releaseKey(base64: string): KeyObject {
    const raw = Buffer.from(base64.trim(), 'base64');
    if (raw.length !== 65 || raw[0] !== 0x04)
        throw new Error('发布公钥不是 65 字节的 P-256 公钥');
    return createPublicKey({
        key: { kty: 'EC', crv: 'P-256', x: raw.subarray(1, 33).toString('base64url'), y: raw.subarray(33).toString('base64url') },
        format: 'jwk',
    });
}

export function signatureValid(key: KeyObject, text: string, signature: string): boolean {
    const bytes = Buffer.from(signature, 'base64');
    return bytes.length === 64 && verify('sha256', Buffer.from(text), { key, dsaEncoding: 'ieee-p1363' }, bytes);
}

// 存在 directory 里：<平台>/<版本>/ 下面是安装包和 release.json，<平台>/latest.json 是现在的那一个
export class Releases {
    readonly #directory: string;
    readonly #key: KeyObject;

    constructor(directory: string, key: KeyObject) {
        this.#directory = directory;
        this.#key = key;
    }

    async latest(platform: Platform): Promise<Release | null> {
        return this.#read(path.join(this.#directory, platform, 'latest.json'));
    }

    // 新的在前
    async list(platform: Platform): Promise<Release[]> {
        let versions: string[];
        try {
            versions = (await readdir(path.join(this.#directory, platform))).filter(isVersion);
        } catch {
            return [];
        }
        const releases = await Promise.all(versions.map((v) => this.#read(path.join(this.#directory, platform, v, 'release.json'))));
        return releases.filter((r): r is Release => r !== null).sort((a, b) => compareVersions(b.version, a.version));
    }

    // 安装包的路径；没有这个版本、文件名对不上时 null
    async file(platform: string, version: string, file: string): Promise<string | null> {
        if (!isPlatform(platform) || !isVersion(version) || file !== FILES[platform](version))
            return null;
        const full = path.join(this.#directory, platform, version, file);
        try {
            return (await stat(full)).isFile() ? full : null;
        } catch {
            return null;
        }
    }

    // 收下一个新版本：边收边算 sha256，收完核对签名、比现在的新，再换成现在的那一个
    async publish(platform: string, version: string, signature: string, body: Readable): Promise<Release> {
        if (!isPlatform(platform))
            throw new ReleaseError('platform');
        if (!isVersion(version))
            throw new ReleaseError('version');
        const folder = path.join(this.#directory, platform);
        await mkdir(folder, { recursive: true });
        const upload = path.join(folder, `.upload-${randomBytes(8).toString('hex')}`);
        try {
            const hash = createHash('sha256');
            let size = 0;
            await pipeline(body, async function* (chunks: AsyncIterable<Buffer>) {
                for await (const chunk of chunks) {
                    hash.update(chunk);
                    size += chunk.length;
                    yield chunk;
                }
            }, createWriteStream(upload, { mode: 0o644 }));
            if (size === 0)
                throw new ReleaseError('empty');
            const sha256 = hash.digest('hex');
            if (!signatureValid(this.#key, releaseText({ platform, version, size, sha256 }), signature))
                throw new ReleaseError('signature');
            const current = await this.latest(platform);
            if (current && compareVersions(version, current.version) <= 0)
                throw new ReleaseError('notNewer');
            const release: Release = {
                platform, version, size, sha256, signature, file: FILES[platform](version), publishedAt: new Date().toISOString(),
            };
            const target = path.join(folder, version);
            await rm(target, { recursive: true, force: true });
            await mkdir(target);
            await rename(upload, path.join(target, release.file));
            await this.#write(path.join(target, 'release.json'), release);
            await this.#write(path.join(folder, 'latest.json'), release);
            return release;
        } finally {
            await rm(upload, { force: true });
        }
    }

    // 现在的那一个不删（程序正要去下载它）
    async remove(platform: string, version: string): Promise<'removed' | 'current' | 'notFound'> {
        if (!isPlatform(platform) || !isVersion(version))
            return 'notFound';
        if ((await this.latest(platform))?.version === version)
            return 'current';
        const target = path.join(this.#directory, platform, version);
        try {
            await stat(target);
        } catch {
            return 'notFound';
        }
        await rm(target, { recursive: true });
        return 'removed';
    }

    async #read(file: string): Promise<Release | null> {
        try {
            return JSON.parse(await readFile(file, 'utf8')) as Release;
        } catch {
            return null;
        }
    }

    // 先写到旁边再换过去：读的一方不会读到写了一半的
    async #write(file: string, release: Release): Promise<void> {
        const partial = `${file}.partial`;
        await writeFile(partial, `${JSON.stringify(release, null, 2)}\n`);
        await rename(partial, file);
    }
}
