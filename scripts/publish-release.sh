#!/bin/sh
# 把签好的安装包发布到正式的服务器，和管理后台的"发布新版本"一样（服务器核对签名、确认比现在的新，
# 再换成现在的那一个；docs/admin.md）：
#
#   node scripts/sign-release.mjs <安装包>      先签名，旁边写出 <安装包>.release.json
#   scripts/publish-release.sh <安装包>
#
# RC_HOST：服务器（默认 ubuntu@bilindesk.com），要能用 SSH 密钥登录、有免密 sudo
set -eu

file="$1"
host="${RC_HOST:-ubuntu@bilindesk.com}"
field() {
    node -e 'console.log(JSON.parse(require("fs").readFileSync(process.argv[1], "utf8"))[process.argv[2]])' \
        "$file.release.json" "$1"
}
platform=$(field platform)
version=$(field version)
signature=$(field signature)

# 安装包经标准输入交给管理后台的容器（它能写 data/files/updates），在那里核对签名、放好（脚本里只用双引号）
script='
import { readFileSync } from "node:fs";
import { Releases, releaseKey } from "/app/dist/releases.js";
const [platform, version, signature] = process.argv.slice(1);
const releases = new Releases("/var/lib/bilin/updates", releaseKey(readFileSync("/app/release-key.pub", "utf8")));
const release = await releases.publish(platform, version, signature, process.stdin);
console.log("发布了", release.platform, release.version, release.sha256);
'
ssh -o BatchMode=yes "$host" "cd /srv/bilin && sudo docker compose exec -T admin \
    node --input-type=module -e '$script' '$platform' '$version' '$signature'" < "$file"
