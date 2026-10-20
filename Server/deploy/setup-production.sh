#!/bin/sh
# 在正式的服务器上装好服务端要的东西（Rocky Linux 9）：Node 24、PM2、Nginx、Redis 7、MongoDB 8、coturn、certbot，
# 服务端的用户和目录、中继的共享密钥、防火墙放行的端口、日志只留 14 天，再给域名申请 Let's Encrypt 的证书。
# 之后用 deploy-production.sh 部署。再跑一遍也行：装过的不重装，密钥不换，证书没到期不重新申请。
#
# 一台服务器可以放好几个网站：每个网站在 /etc/nginx/conf.d 里有自己的一份配置（按域名起名），不认识的域名、
# 直接用 IP 地址来的由 00-default.conf 回答。服务器上原有的东西不动：防火墙只加端口、不重新加载。
# https 的 443 由服务器上的 Xray（VPN，Reality，另外装的）收下再转给 Nginx 的 127.0.0.1:4443（nginx/01-realip.conf）。
#
# 先要：域名的 A 记录指到服务器（不经 CDN 代理：中继要直接连到这台服务器）；云服务商控制台里的防火墙（安全组）
# 放行 TCP 80、443，TCP 和 UDP 3478，UDP 49152-49407
#
#   Server/deploy/setup-production.sh
#   RC_HOST=root@另一台 RC_DOMAIN=它的域名 RC_PUBLIC_IP=它的公网地址 Server/deploy/setup-production.sh
set -eu

host="${RC_HOST:-root@bilindesk.com}"
domain="${RC_DOMAIN:-bilindesk.com}"
public_ip="${RC_PUBLIC_IP:-43.108.15.225}"
deploy="$(cd "$(dirname "$0")" && pwd)"

scp -q "$deploy/nginx/nginx.conf" "$host:/tmp/bilin-nginx.conf"
scp -q "$deploy/nginx/00-default.conf" "$host:/tmp/bilin-00-default.conf"

ssh "$host" bash -s -- "$domain" "$public_ip" <<'EOF'
set -eu
domain="$1"
public_ip="$2"
# 云服务器在 NAT 后面：本机只看得到内网地址，公网地址是映射过来的
private_ip=$(ip -4 -o addr show eth0 | awk '{print $4}' | cut -d/ -f1)

dnf -y -q module enable nodejs:24 nginx:1.26 redis:7
dnf -y -q install epel-release
dnf -y -q install nodejs npm nginx redis rsync coturn coturn-utils openssl certbot
cat > /etc/yum.repos.d/mongodb-org-8.0.repo <<'REPO'
[mongodb-org-8.0]
name=MongoDB Repository
baseurl=https://repo.mongodb.org/yum/redhat/9/mongodb-org/8.0/$basearch/
gpgcheck=1
enabled=1
gpgkey=https://pgp.mongodb.com/server-8.0.asc
REPO
dnf -y -q install mongodb-org
command -v pm2 >/dev/null || npm install -g -s pm2

# 机器小（不到 1 GB 内存）：MongoDB 的缓存只给 256 MB。只听本机
cat > /etc/mongod.conf <<'CONF'
# 由 Server/deploy/setup-production.sh 生成，要改就改它再跑一遍
systemLog:
  destination: file
  logAppend: true
  path: /var/log/mongodb/mongod.log
storage:
  dbPath: /var/lib/mongo
  wiredTiger:
    engineConfig:
      cacheSizeGB: 0.25
processManagement:
  timeZoneInfo: /usr/share/zoneinfo
net:
  port: 27017
  bindIp: 127.0.0.1
CONF
systemctl enable -q --now mongod redis
systemctl restart mongod

# 服务端用自己的用户运行，不用 root。程序在 /opt/bilin，数据（发布的新版本、备份）在 /var/lib/bilin，
# 密钥在 /etc/bilin，网页在 /var/www/<域名>
id bilin >/dev/null 2>&1 || useradd --system --create-home --home-dir /opt/bilin --shell /sbin/nologin bilin
install -d -o bilin -g bilin /opt/bilin/server
install -d -o bilin -g bilin -m 750 /var/lib/bilin /var/lib/bilin/backups /var/lib/bilin/updates
install -d -m 755 /etc/bilin /var/log/bilin "/var/www/$domain" /var/www/letsencrypt
install -d -o coturn -g coturn /var/log/bilin/coturn
# PM2 跟着开机启动（pm2-bilin.service）
env PATH="$PATH" pm2 startup systemd -u bilin --hp /opt/bilin >/dev/null

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
cat > /etc/coturn/turnserver.conf <<CONF
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
chown root:coturn /etc/coturn/turnserver.conf
chmod 640 /etc/coturn/turnserver.conf
systemctl enable -q coturn
systemctl restart coturn

# 防火墙：只加这几个端口，运行中的和开机时的都加；不重新加载，别的规则不动
for port in 80/tcp 443/tcp 3478/tcp 3478/udp 49152-49407/udp; do
    firewall-cmd -q --query-port="$port" || firewall-cmd -q --add-port="$port"
    firewall-cmd -q --permanent --query-port="$port" || firewall-cmd -q --permanent --add-port="$port"
done

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

# Nginx：自带的默认网站换成 00-default.conf（只回答申请证书的验证）
install -m 644 /tmp/bilin-nginx.conf /etc/nginx/nginx.conf
install -m 644 /tmp/bilin-00-default.conf /etc/nginx/conf.d/00-default.conf
rm -f /tmp/bilin-nginx.conf /tmp/bilin-00-default.conf
nginx -t -q
systemctl enable -q nginx
systemctl reload-or-restart nginx

# 证书：Let's Encrypt，经 80 端口验证（文件放在 /var/www/letsencrypt）。certbot 的定时任务自己续期，续好了重新加载 Nginx
certbot certonly --webroot -w /var/www/letsencrypt -d "$domain" -d "www.$domain" \
    --non-interactive --agree-tos --register-unsafely-without-email --keep-until-expiring --quiet
install -d /etc/letsencrypt/renewal-hooks/deploy
cat > /etc/letsencrypt/renewal-hooks/deploy/reload-nginx <<'HOOK'
#!/bin/sh
# 由 Server/deploy/setup-production.sh 生成：证书续好了，Nginx 换上新的
systemctl reload nginx
HOOK
chmod 755 /etc/letsencrypt/renewal-hooks/deploy/reload-nginx
systemctl enable -q --now certbot-renew.timer

node -v
echo "$domain：公网 $public_ip，内网 $private_ip，证书到 $(openssl x509 -enddate -noout -in "/etc/letsencrypt/live/$domain/cert.pem" | cut -d= -f2)"
EOF
