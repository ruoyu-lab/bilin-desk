#!/usr/bin/env node
// 给安装包签名，好在管理后台发布成新版本（docs/admin.md 的"发布新版本"）：
//
//   node scripts/sign-release.mjs new-key        第一次：生成发布密钥，公钥写进 Server/release-key.pub
//   node scripts/sign-release.mjs <安装包>        签名，旁边写出 <安装包>.release.json，和安装包一起在后台上传
//
// 私钥在 ~/.bilin/release-key.pem（环境变量 RC_RELEASE_KEY 可以换），只在这台电脑上，不进仓库：
// 丢了就发不了程序认的新版本，要备份好。版本号是 CMakeLists.txt 里的 project VERSION（安装包就是按它编的）。
// 签的内容和 Server/src/releases.ts、Core/src/rc/release.cpp 的一样
import { createHash, createPrivateKey, createPublicKey, generateKeyPairSync, sign } from 'node:crypto';
import { createReadStream, existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { homedir } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const keyFile = process.env.RC_RELEASE_KEY ?? path.join(homedir(), '.bilin', 'release-key.pem');
const publicKeyFile = path.join(root, 'Server', 'release-key.pub');

function rawPublicKey(key) {
    const jwk = createPublicKey(key).export({ format: 'jwk' });
    return Buffer.concat([Buffer.from([4]), Buffer.from(jwk.x, 'base64url'), Buffer.from(jwk.y, 'base64url')]);
}

function newKey() {
    if (existsSync(keyFile)) {
        console.error(`已经有发布密钥了：${keyFile}。换密钥以后旧版本的程序不再认新发布的安装包`);
        process.exit(1);
    }
    const { privateKey } = generateKeyPairSync('ec', { namedCurve: 'P-256' });
    mkdirSync(path.dirname(keyFile), { recursive: true, mode: 0o700 });
    writeFileSync(keyFile, privateKey.export({ format: 'pem', type: 'pkcs8' }), { mode: 0o600 });
    writeFileSync(publicKeyFile, `${rawPublicKey(privateKey).toString('base64')}\n`);
    console.log(`私钥：${keyFile}（备份好，丢了就发不了新版本）`);
    console.log(`公钥：${publicKeyFile}（提交进仓库，程序和服务器都用它）`);
}

function platformOf(file) {
    if (file.endsWith('.pkg'))
        return 'macos-arm64';
    if (file.endsWith('.exe'))
        return 'windows-x64';
    console.error('不认识的安装包：要 .pkg（macOS）或 .exe（Windows）');
    process.exit(2);
}

// 和程序里的一样：project VERSION 加上阶段（BILIN_VERSION_LABEL，比如 0.1.0-beta）
function projectVersion() {
    const cmake = readFileSync(path.join(root, 'CMakeLists.txt'), 'utf8');
    const version = /^project\(Bilin VERSION ([0-9.]+)/m.exec(cmake)?.[1];
    if (!version) {
        console.error('CMakeLists.txt 里找不到 project VERSION');
        process.exit(1);
    }
    const label = /^set\(BILIN_VERSION_LABEL ([a-z]*)\)/m.exec(cmake)?.[1];
    return label ? `${version}-${label}` : version;
}

async function signRelease(file) {
    const key = createPrivateKey(readFileSync(keyFile));
    // 签之前核对一下：私钥和仓库里的公钥是一对（不然发出去的程序不认）
    if (!rawPublicKey(key).equals(Buffer.from(readFileSync(publicKeyFile, 'utf8').trim(), 'base64'))) {
        console.error(`${keyFile} 和 ${publicKeyFile} 不是一对`);
        process.exit(1);
    }
    const platform = platformOf(file);
    const version = projectVersion();
    const hash = createHash('sha256');
    let size = 0;
    for await (const chunk of createReadStream(file)) {
        hash.update(chunk);
        size += chunk.length;
    }
    const sha256 = hash.digest('hex');
    const text = `bilin-release\nplatform=${platform}\nversion=${version}\nsize=${size}\nsha256=${sha256}\n`;
    const signature = sign('sha256', Buffer.from(text), { key, dsaEncoding: 'ieee-p1363' }).toString('base64');
    const output = `${file}.release.json`;
    writeFileSync(output, `${JSON.stringify({ platform, version, size, sha256, signature }, null, 2)}\n`);
    console.log(`${platform} ${version}，${(size / 1048576).toFixed(1)} MB`);
    console.log(`签好了：${output}（在管理后台和安装包一起上传）`);
}

const argument = process.argv[2];
if (argument === 'new-key')
    newKey();
else if (argument)
    await signRelease(argument);
else {
    console.error('用法：node scripts/sign-release.mjs new-key | <安装包>');
    process.exit(2);
}
