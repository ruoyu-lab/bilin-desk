#!/bin/sh
# 把服务端、管理后台、官网部署到正式的服务器（setup-production.sh 装好的）。在这台电脑上编译，只把运行要的文件
# 传上去：服务端编出来的 dist、package*.json、PM2 的配置、发布公钥；官网和管理后台编出来的网页。
# 服务器上证书补上缺的名字、装依赖、PM2 平滑重启两个实例和管理后台、装 Nginx 的配置（每个域名一份）
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

# 要传的东西先放在一起：服务端的放进 /opt/bilin/server，网页放进 /var/www/<域名>（官网 bilindesk.com，
# 管理后台 admin.bilindesk.com）
stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT
mkdir -p "$stage/server" "$stage/www" "$stage/admin"
cp -R "$server/dist" "$stage/server/dist"
cp "$server/package.json" "$server/package-lock.json" "$server/ecosystem.config.cjs" "$server/release-key.pub" \
    "$stage/server/"
cp -R "$root/Website/dist/." "$stage/www/"
cp -R "$server/admin/dist/." "$stage/admin/"

# node_modules 在服务器上装（--delete 不删它）。文件的所有者、权限传完以后在服务器上设（macOS 的 rsync 设不了）
rsync -a --delete --exclude node_modules "$stage/server/" "$host:/opt/bilin/server/"
rsync -a --delete "$stage/www/" "$host:/var/www/$domain/"
rsync -a --delete "$stage/admin/" "$host:/var/www/admin.$domain/"
scp -q "$server/deploy/nginx/$domain.conf" "$host:/etc/nginx/conf.d/$domain.conf"
scp -q "$server/deploy/nginx/admin.$domain.conf" "$host:/etc/nginx/conf.d/admin.$domain.conf"

ssh "$host" bash -s -- "$domain" <<'EOF'
set -eu
domain="$1"
chown -R bilin:bilin /opt/bilin/server
chown -R root:root "/var/www/$domain" "/var/www/admin.$domain"
chmod -R u=rwX,go=rX "/var/www/$domain" "/var/www/admin.$domain"
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
