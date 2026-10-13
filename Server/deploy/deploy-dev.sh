#!/bin/sh
# 把服务端和管理后台的网页部署到开发用的虚拟机（setup-dev.sh 建的）：在 Mac 上编译，虚拟机里从 Mac 的目录
# 同步运行要的文件（同 deploy-production.sh）、装依赖、PM2 平滑重启两个实例和管理后台、装 Nginx 的配置
#
#   Server/deploy/deploy-dev.sh
set -eu

machine="${RC_MACHINE:-bilin-dev}"
server="$(cd "$(dirname "$0")/.." && pwd)"

for package in "$server" "$server/admin"; do
    npm --prefix "$package" ci --no-audit --no-fund --loglevel=error
    npm --prefix "$package" run build --silent
done

# 虚拟机里看得到 Mac 的目录
orb -m "$machine" bash -s -- "$server" <<'EOF2'
set -eu
source="$1"
target=/opt/bilin/server

sudo install -d -o "$(id -u)" -g "$(id -g)" /opt/bilin "$target"
# 管理后台的数据（备份、发布的新版本）
sudo install -d -o "$(id -u)" -g "$(id -g)" -m 750 /var/lib/bilin /var/lib/bilin/backups /var/lib/bilin/updates
rsync -a --delete "$source/dist/" "$target/dist/"
cp "$source/package.json" "$source/package-lock.json" "$source/ecosystem.config.cjs" "$source/release-key.pub" "$target/"
sudo install -d /var/www/bilin-dev
sudo rsync -a --delete --chmod=D755,F644 "$source/admin/dist/" /var/www/bilin-dev/admin/
cd "$target"
npm ci --omit=dev --no-audit --no-fund --loglevel=error

sudo cp "$source/deploy/nginx/dev.conf" /etc/nginx/conf.d/bilin.conf
sudo nginx -t -q
sudo systemctl reload nginx

pm2 startOrReload ecosystem.config.cjs --env development --update-env
pm2 save --force >/dev/null
sleep 1
curl -fsS http://127.0.0.1:3000/health
echo
EOF2
