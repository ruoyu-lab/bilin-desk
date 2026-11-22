#!/bin/sh
# 在正式的服务器上给比邻建好位置（甲骨文 ARM，Ubuntu 24.04，每个项目在 /srv/<项目> 的容器里，见服务器上的 /srv/README.md）：
# /srv/bilin 的目录、中继的共享密钥和 coturn 的配置、防火墙放行中继的端口。之后用 deploy-production.sh 部署。
# 再跑一遍也行：建过的不重建，密钥不换。
#
# 服务器上已经有的：Docker；/srv/gateway（对外的 Nginx 和续证书的 certbot，整台机器共用）。比邻的站点配置和静态文件
# 由 gateway 读（它的 compose.yml 里挂着 /srv/bilin/nginx、/srv/bilin/www），证书也由它申请：
#   cd /srv/gateway && sudo docker compose run --rm certbot certonly --webroot -w /var/www/letsencrypt --cert-name bilindesk.com \
#       -d bilindesk.com -d www.bilindesk.com -d admin.bilindesk.com -d web.bilindesk.com
#
# 先要：域名的 A 记录指到服务器（不经 CDN 代理：中继要直接连到这台服务器）；甲骨文的安全列表放行 TCP 80、443，
# TCP 和 UDP 3478，UDP 49152-49407
#
#   Server/deploy/setup-production.sh
#   RC_HOST=ubuntu@另一台 RC_PUBLIC_IP=它的公网地址 Server/deploy/setup-production.sh
set -eu

host="${RC_HOST:-ubuntu@bilindesk.com}"
public_ip="${RC_PUBLIC_IP:-217.142.231.189}"

ssh "$host" sudo bash -s -- "$public_ip" <<'EOF'
set -eu
public_ip="$1"
# 甲骨文是 NAT：本机只看得到内网地址，公网地址是映射过来的
private_ip=$(ip -4 -o route get 1.1.1.1 | sed -E 's/.* src ([0-9.]+).*/\1/')
# 程序在容器里用的用户（compose.yml 的 user），coturn 的镜像用 nobody
uid=2003

# 程序（app，只读挂进容器）、静态文件（www，gateway 的 Nginx 直接给）、数据（data）、密钥（secrets）
install -d -m 711 /srv/bilin
install -d -m 755 /srv/bilin/app /srv/bilin/nginx /srv/bilin/admin-image /srv/bilin/www \
    /srv/bilin/www/site /srv/bilin/www/admin /srv/bilin/www/web
install -d -m 700 /srv/bilin/data /srv/bilin/data/mongo /srv/bilin/data/redis
install -d -m 750 -o $uid -g $uid /srv/bilin/data/files /srv/bilin/data/files/updates /srv/bilin/data/files/backups
install -d -m 700 -o $uid -g $uid /srv/bilin/secrets

cd /srv/bilin/secrets
# 中继（coturn，同时是 STUN）的共享密钥：第一次生成，之后不变。凭据由服务端按它签发、有时效（Server/src/ice.ts）
[ -s turn-secret ] || openssl rand -base64 32 > turn-secret
# 后台密码的哈希、发邮件的密钥：先空着（挂进容器的文件得先有），set-admin-password.sh、set-mail-key.sh 写
touch admin-password mail-key
chown $uid:$uid turn-secret admin-password mail-key
chmod 400 turn-secret admin-password mail-key

# coturn 用宿主机的网络。中继地址：本机的内网地址，告诉设备的是公网地址（external-ip）。只在中继地址之间转发：
# 两端都经这台中继，不能被拿来当跳板去连别的机器
cat > turnserver.conf <<CONF
# 由 Server/deploy/setup-production.sh 生成，要改就改它再跑一遍
listening-ip=$private_ip
listening-port=3478
external-ip=$public_ip/$private_ip
relay-ip=$private_ip
min-port=49152
max-port=49407
no-tls
no-dtls
use-auth-secret
static-auth-secret=$(cat turn-secret)
realm=bilin
fingerprint
denied-peer-ip=0.0.0.0-255.255.255.255
denied-peer-ip=::-ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff
allowed-peer-ip=$public_ip
allowed-peer-ip=$private_ip
no-multicast-peers
no-cli
log-file=stdout
simple-log
CONF
chown nobody:nogroup turnserver.conf
chmod 400 turnserver.conf

# 防火墙（iptables，最后一条是 REJECT）：缺的端口插在 REJECT 前面，再存下来开机用
for rule in "-p tcp --dport 3478" "-p udp --dport 3478" "-p udp --dport 49152:49407"; do
    # shellcheck disable=SC2086
    if ! iptables -C INPUT $rule -m state --state NEW -j ACCEPT 2>/dev/null; then
        reject=$(iptables -L INPUT --line-numbers | awk '$2 == "REJECT" {print $1; exit}')
        iptables -I INPUT "$reject" $rule -m state --state NEW -j ACCEPT
    fi
done
netfilter-persistent save >/dev/null 2>&1

grep -q /srv/bilin/nginx /srv/gateway/compose.yml \
    || echo "gateway 还没挂比邻：在 /srv/gateway/compose.yml 的 nginx 下加 /srv/bilin/nginx、/srv/bilin/www 两行（见它开头的说明）"
echo "公网 $public_ip，内网 $private_ip"
EOF
