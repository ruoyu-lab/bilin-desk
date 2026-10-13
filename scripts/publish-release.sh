#!/bin/sh
# 把签好的安装包发布到正式的服务器，和管理后台的"发布新版本"一样（服务器核对签名、确认比现在的新，
# 再换成现在的那一个；docs/admin.md）：
#
#   node scripts/sign-release.mjs <安装包>      先签名，旁边写出 <安装包>.release.json
#   scripts/publish-release.sh <安装包>
#
# RC_HOST：服务器（默认 root@bilindesk.com），要能用 SSH 密钥登录
set -eu

file="$1"
host="${RC_HOST:-root@bilindesk.com}"
field() {
    node -e 'console.log(JSON.parse(require("fs").readFileSync(process.argv[1], "utf8"))[process.argv[2]])' \
        "$file.release.json" "$1"
}
platform=$(field platform)
version=$(field version)
signature=$(field signature)

stage=$(ssh -o BatchMode=yes "$host" 'mktemp -d /tmp/bilin-publish-XXXXXX')
scp -q -o BatchMode=yes "$file" "$host:$stage/package"
ssh -o BatchMode=yes "$host" sh -s -- "$stage" "$platform" "$version" "$signature" <<'REMOTE'
set -u
stage="$1"
chmod 755 "$stage"
chmod 644 "$stage/package"
cd /opt/bilin/server
runuser -u bilin -- node --input-type=module - "$@" <<'JS'
import { createReadStream, readFileSync } from 'node:fs';
import { Releases, releaseKey } from '/opt/bilin/server/dist/releases.js';
const [stage, platform, version, signature] = process.argv.slice(2);
const releases = new Releases('/var/lib/bilin/updates', releaseKey(readFileSync('release-key.pub', 'utf8')));
const release = await releases.publish(platform, version, signature, createReadStream(`${stage}/package`));
console.log('发布了', release.platform, release.version, release.sha256);
JS
status=$?
rm -rf "$stage"
exit $status
REMOTE
