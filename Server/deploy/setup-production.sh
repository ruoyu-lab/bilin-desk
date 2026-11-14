#!/bin/sh
# 在正式的服务器上装好服务端要的东西（甲骨文 ARM，Ubuntu 24.04）：MongoDB 8、比邻自己的 Redis、coturn，
# 服务端的用户和目录、PM2 开机启动、中继的共享密钥、防火墙放行的端口、日志只留 14 天，再给域名申请 Let's Encrypt 的证书。
# 之后用 deploy-production.sh 部署。再跑一遍也行：装过的不重装，密钥不换，证书没到期不重新申请。
#
# 这台服务器和别的网站同住：Node 24、PM2、Nginx（nginx.org 的）、certbot 已经装好，nginx.conf 和别的网站的配置不动，
# 比邻只往 conf.d 里加自己的（按域名起名）；Redis 另起一个（127.0.0.1:6379，bilin-redis.service），PM2 用自己的
# （PM2_HOME=/opt/bilin/.pm2，pm2-bilin.service）。Nginx 自己听 443。
#
# 先要：域名的 A 记录指到服务器（不经 CDN 代理：中继要直接连到这台服务器）；甲骨文的安全列表放行 TCP 80、443，
# TCP 和 UDP 3478，UDP 49152-49407
#
#   Server/deploy/setup-production.sh
#   RC_HOST=ubuntu@另一台 RC_DOMAIN=它的域名 RC_PUBLIC_IP=它的公网地址 Server/deploy/setup-production.sh
set -eu

host="${RC_HOST:-ubuntu@bilindesk.com}"
domain="${RC_DOMAIN:-bilindesk.com}"
public_ip="${RC_PUBLIC_IP:-217.142.231.189}"

ssh "$host" sudo bash -s -- "$domain" "$public_ip" <<'EOF'
set -eu
domain="$1"
public_ip="$2"
# 甲骨文是 NAT：本机只看得到内网地址，公网地址是映射过来的
private_ip=$(ip -4 -o route get 1.1.1.1 | sed -E 's/.* src ([0-9.]+).*/\1/')
export DEBIAN_FRONTEND=noninteractive

if [ ! -f /etc/apt/sources.list.d/mongodb-org-8.0.list ]; then
    curl -fsSL https://pgp.mongodb.com/server-8.0.asc | gpg --dearmor -o /usr/share/keyrings/mongodb-server-8.0.gpg
    echo "deb [arch=arm64,amd64 signed-by=/usr/share/keyrings/mongodb-server-8.0.gpg] https://repo.mongodb.org/apt/ubuntu noble/mongodb-org/8.0 multiverse" \
        > /etc/apt/sources.list.d/mongodb-org-8.0.list
    apt-get -qq update
fi
apt-get -qq install -y mongodb-org coturn rsync >/dev/null

# MongoDB 的缓存给 1 GB（机器 12 GB，和别的服务同住）。只听本机
cat > /etc/mongod.conf <<'CONF'
# 由 Server/deploy/setup-production.sh 生成，要改就改它再跑一遍
systemLog:
  destination: file
  logAppend: true
  path: /var/log/mongodb/mongod.log
storage:
  dbPath: /var/lib/mongodb
  wiredTiger:
    engineConfig:
      cacheSizeGB: 1
processManagement:
  timeZoneInfo: /usr/share/zoneinfo
net:
  port: 27017
  bindIp: 127.0.0.1
CONF
systemctl enable -q mongod
systemctl restart mongod

# 服务端用自己的用户运行，不用 root。程序在 /opt/bilin，数据（发布的新版本、备份）在 /var/lib/bilin，
# 密钥在 /etc/bilin，网页在 /var/www/<域名>
id bilin >/dev/null 2>&1 || useradd --system --create-home --home-dir /opt/bilin --shell /usr/sbin/nologin bilin
install -d -o bilin -g bilin /opt/bilin/server
install -d -o bilin -g bilin -m 750 /var/lib/bilin /var/lib/bilin/backups /var/lib/bilin/updates
install -d -m 755 /etc/bilin /var/log/bilin "/var/www/$domain" "/var/www/admin.$domain" "/var/www/web.$domain" /var/www/letsencrypt
install -d -o turnserver -g turnserver /var/log/bilin/coturn

# 比邻自己的 Redis：127.0.0.1:6379，存在线状态、统计、网页版的会话。数据定时存盘（/var/lib/bilin-redis）
cat > /etc/bilin/redis.conf <<'CONF'
# 由 Server/deploy/setup-production.sh 生成，要改就改它再跑一遍
bind 127.0.0.1 -::1
port 6379
supervised systemd
dir /var/lib/bilin-redis
save 3600 1 300 100 60 10000
appendonly no
maxmemory 256mb
CONF
cat > /etc/systemd/system/bilin-redis.service <<'UNIT'
# 由 Server/deploy/setup-production.sh 生成
[Unit]
Description=Bilin Redis (127.0.0.1:6379)
After=network.target

[Service]
Type=notify
User=redis
Group=redis
ExecStart=/usr/bin/redis-server /etc/bilin/redis.conf
StateDirectory=bilin-redis
Restart=always
LimitNOFILE=10032

[Install]
WantedBy=multi-user.target
UNIT
systemctl daemon-reload
systemctl enable -q --now bilin-redis

# PM2 跟着开机启动（pm2-bilin.service，PM2_HOME=/opt/bilin/.pm2）
pm2 startup systemd -u bilin --hp /opt/bilin >/dev/null

# 中继（coturn，同时是 STUN）：3478 端口，UDP 和 TCP。凭据由服务端按共享密钥签发、有时效（Server/src/ice.ts）。
# 共享密钥只在服务器上：第一次生成，之后不变
secret=/etc/bilin/turn-secret
if [ ! -s "$secret" ]; then
    openssl rand -base64 32 > "$secret"
fi
chown bilin:bilin "$secret"
chmod 600 "$secret"
# 中继地址：本机的内网地址，告诉设备的是公网地址（external-ip）。只在中继地址之间转发：两端都经这台中继，
# 不能被拿来当跳板去连别的机器
cat > /etc/turnserver.conf <<CONF
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
static-auth-secret=$(cat "$secret")
realm=bilin
fingerprint
denied-peer-ip=0.0.0.0-255.255.255.255
denied-peer-ip=::-ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff
allowed-peer-ip=$public_ip
allowed-peer-ip=$private_ip
no-multicast-peers
no-cli
log-file=/var/log/bilin/coturn/turnserver.log
simple-log
CONF
chown root:turnserver /etc/turnserver.conf
chmod 640 /etc/turnserver.conf
systemctl enable -q coturn
systemctl restart coturn

# 防火墙（iptables，最后一条是 REJECT）：缺的端口插在 REJECT 前面，再存下来开机用
for rule in "-p tcp --dport 3478" "-p udp --dport 3478" "-p udp --dport 49152:49407"; do
    # shellcheck disable=SC2086
    if ! iptables -C INPUT $rule -m state --state NEW -j ACCEPT 2>/dev/null; then
        reject=$(iptables -L INPUT --line-numbers | awk '$2 == "REJECT" {print $1; exit}')
        iptables -I INPUT "$reject" $rule -m state --state NEW -j ACCEPT
    fi
done
netfilter-persistent save >/dev/null 2>&1

# 日志只留 14 天（隐私政策里写的）
cat > /etc/logrotate.d/bilin <<'CONF'
/var/log/bilin/*.log /var/log/bilin/coturn/*.log {
    daily
    rotate 14
    missingok
    notifempty
    compress
    delaycompress
    copytruncate
}
/opt/bilin/.pm2/logs/*.log {
    su bilin bilin
    daily
    rotate 14
    missingok
    notifempty
    compress
    delaycompress
    copytruncate
}
CONF

# 证书：Let's Encrypt，一张带上几个名字，经 80 端口验证（文件放在 /var/www/letsencrypt）。第一次网站的配置还没装，
# 先装一份只回答验证的；deploy-production.sh 再换成完整的。certbot 的定时任务自己续期，续好了重新加载 Nginx
site=/etc/nginx/conf.d/$domain.conf
if [ ! -f "$site" ]; then
    cat > "$site" <<CONF
# 由 Server/deploy/setup-production.sh 临时装上，deploy-production.sh 换成完整的
server {
    listen 80;
    listen [::]:80;
    server_name $domain www.$domain admin.$domain web.$domain;
    location /.well-known/acme-challenge/ {
        root /var/www/letsencrypt;
    }
}
CONF
    nginx -t -q
    systemctl reload nginx
fi
certbot certonly --webroot -w /var/www/letsencrypt --cert-name "$domain" \
    -d "$domain" -d "www.$domain" -d "admin.$domain" -d "web.$domain" \
    --deploy-hook "systemctl reload nginx" \
    --non-interactive --agree-tos --register-unsafely-without-email --keep-until-expiring --quiet

node -v
echo "$domain：公网 $public_ip，内网 $private_ip，证书到 $(openssl x509 -enddate -noout -in "/etc/letsencrypt/live/$domain/cert.pem" | cut -d= -f2)"
EOF
