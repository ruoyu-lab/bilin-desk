#!/bin/sh
# 把服务端、管理后台、官网、网页版部署到正式的服务器（/srv/bilin，setup-production.sh 建好的）。在这台电脑上编译，
# 只把运行要的文件传上去：服务端编出来的 dist、package*.json、发布公钥放进 app/；官网、管理后台、网页版编出来的网页
# 放进 www/{site,admin,web}（网页版带 SDK 的浏览器包，scripts/build-web-sdk.sh 编，要 Emscripten）；
# 容器的配置（production/compose.yml、admin-image/）和站点配置（production/nginx/bilin.conf）也照仓库里的放上去。
# 服务器上在容器里装依赖（服务器是 ARM 的 Linux），起好容器，服务端两个实例一个一个重启（总有一个在），gateway 的
# Nginx 重新读配置。登录服务器的用户有免密 sudo
#
#   Server/deploy/deploy-production.sh
#   RC_HOST=ubuntu@另一台 Server/deploy/deploy-production.sh
set -eu

host="${RC_HOST:-ubuntu@bilindesk.com}"
root="$(cd "$(dirname "$0")/../.." && pwd)"
server="$root/Server"
production="$server/deploy/production"

build_package() {
    npm --prefix "$1" ci --no-audit --no-fund --loglevel=error
    npm --prefix "$1" run build --silent
}
build_package "$server"
build_package "$server/admin"
build_package "$root/Website"
"$root/scripts/build-web-sdk.sh" >/dev/null
build_package "$root/Web"

stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT
mkdir -p "$stage/app"
cp -R "$server/dist" "$stage/app/dist"
cp "$server/package.json" "$server/package-lock.json" "$server/release-key.pub" "$stage/app/"

# 经 sudo 的 rsync 写；node_modules 在服务器上装（--delete 不删它）。所有者、权限传完以后在服务器上设
as_root() {
    rsync --rsync-path="sudo rsync" "$@"
}
as_root -a --delete --exclude node_modules "$stage/app/" "$host:/srv/bilin/app/"
as_root -a --delete "$root/Website/dist/" "$host:/srv/bilin/www/site/"
as_root -a --delete "$server/admin/dist/" "$host:/srv/bilin/www/admin/"
as_root -a --delete "$root/Web/dist/" "$host:/srv/bilin/www/web/"
as_root -a "$production/compose.yml" "$host:/srv/bilin/compose.yml"
as_root -a --delete "$production/admin-image/" "$host:/srv/bilin/admin-image/"
as_root -a --delete "$production/nginx/" "$host:/srv/bilin/nginx/"

ssh "$host" sudo bash -s <<'EOF'
set -eu
cd /srv/bilin
chown -R root:root app www nginx admin-image compose.yml
chmod -R u=rwX,go=rX app www nginx admin-image compose.yml
docker run --rm -v /srv/bilin/app:/app -w /app node:24-slim \
    npm ci --omit=dev --no-audit --no-fund --loglevel=error

# 配置变了的容器重建；程序是挂进去的，换了它要重启：服务端一个好了再重启另一个
docker compose up -d --build --remove-orphans --quiet-pull
healthy() {
    for _ in $(seq 30); do
        [ "$(docker inspect -f '{{.State.Health.Status}}' "$(docker compose ps -q "$1")")" = healthy ] && return
        sleep 2
    done
    echo "$1 没起来：sudo docker compose logs $1"
    exit 1
}
for service in server-1 server-2; do
    docker compose restart "$service" >/dev/null
    healthy "$service"
done
docker compose restart admin relay >/dev/null

cd /srv/gateway
docker compose exec -T nginx nginx -t -q
docker compose exec -T nginx nginx -s reload
curl -fsS http://127.0.0.1:3000/health
echo
EOF
