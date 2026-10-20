#!/bin/sh
# 把服务端、管理后台、官网、网页版部署到正式的服务器（setup-production.sh 装好的）。在这台电脑上编译，只把运行要的文件
# 传上去：服务端编出来的 dist、package*.json、PM2 的配置、发布公钥；官网、管理后台、网页版编出来的网页（网页版带
# SDK 的浏览器包，scripts/build-web-sdk.sh 编，要 Emscripten）。
# 服务器上证书补上缺的名字、装依赖、PM2 平滑重启两个实例和管理后台、装 Nginx 的配置（每个域名一份）。
# 服务器上 443 是 Xray（VPN，Reality），网站的 https 由它转给 Nginx 的 127.0.0.1:4443（nginx/01-realip.conf）
#
#   Server/deploy/deploy-production.sh
#   RC_HOST=root@另一台 Server/deploy/deploy-production.sh
set -eu

host="${RC_HOST:-root@bilindesk.com}"
domain=bilindesk.com
root="$(cd "$(dirname "$0")/../.." && pwd)"
server="$root/Server"

build_package() {
    npm --prefix "$1" ci --no-audit --no-fund --loglevel=error
    npm --prefix "$1" run build --silent
}
build_package "$server"
build_package "$server/admin"
build_package "$root/Website"
"$root/scripts/build-web-sdk.sh" >/dev/null
build_package "$root/Web"

# 要传的东西先放在一起：服务端的放进 /opt/bilin/server，网页放进 /var/www/<域名>（官网 bilindesk.com，
# 管理后台 admin.bilindesk.com，网页版 web.bilindesk.com）
stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT
mkdir -p "$stage/server" "$stage/www" "$stage/admin" "$stage/web"
cp -R "$server/dist" "$stage/server/dist"
cp "$server/package.json" "$server/package-lock.json" "$server/ecosystem.config.cjs" "$server/release-key.pub" \
    "$stage/server/"
cp -R "$root/Website/dist/." "$stage/www/"
cp -R "$server/admin/dist/." "$stage/admin/"
cp -R "$root/Web/dist/." "$stage/web/"

# node_modules 在服务器上装（--delete 不删它）。文件的所有者、权限传完以后在服务器上设（macOS 的 rsync 设不了）
rsync -a --delete --exclude node_modules "$stage/server/" "$host:/opt/bilin/server/"
rsync -a --delete "$stage/www/" "$host:/var/www/$domain/"
rsync -a --delete "$stage/admin/" "$host:/var/www/admin.$domain/"
rsync -a --delete "$stage/web/" "$host:/var/www/web.$domain/"
for site in "$domain" "admin.$domain" "web.$domain"; do
    scp -q "$server/deploy/nginx/$site.conf" "$host:/etc/nginx/conf.d/$site.conf"
done
scp -q "$server/deploy/nginx/01-realip.conf" "$host:/etc/nginx/conf.d/01-realip.conf"

ssh "$host" bash -s -- "$domain" <<'EOF'
set -eu
domain="$1"
chown -R bilin:bilin /opt/bilin/server
chown -R root:root "/var/www/$domain" "/var/www/admin.$domain" "/var/www/web.$domain"
chmod -R u=rwX,go=rX "/var/www/$domain" "/var/www/admin.$domain" "/var/www/web.$domain"
# 证书：一张带上所有的名字，缺了就补上（新名字由 00-default.conf 回答验证）；都有、还没到期时什么也不做
certbot certonly --webroot -w /var/www/letsencrypt --cert-name "$domain" \
    -d "$domain" -d "www.$domain" -d "admin.$domain" -d "web.$domain" \
    --expand --non-interactive --agree-tos --register-unsafely-without-email --keep-until-expiring --quiet
as_bilin() {
    runuser -u bilin -- env HOME=/opt/bilin PATH="$PATH" "$@"
}
cd /opt/bilin/server
as_bilin npm ci --omit=dev --no-audit --no-fund --loglevel=error

nginx -t -q
systemctl reload nginx

as_bilin pm2 startOrReload ecosystem.config.cjs --update-env
as_bilin pm2 save --force >/dev/null
sleep 2
curl -fsS http://127.0.0.1:3000/health
echo
EOF
