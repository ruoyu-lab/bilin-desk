#!/bin/sh
# 在 Mac 上建开发用的服务器：OrbStack 的 Rocky Linux 9 虚拟机，装 Node 24、PM2、Nginx、Redis、MongoDB 8、coturn。
# 已经建好的虚拟机再跑一遍也行（装过的不会重装，coturn 的配置按虚拟机现在的地址重写）。之后用 deploy-dev.sh 部署
#
#   Server/deploy/setup-dev.sh
set -eu

machine="${RC_MACHINE:-bilin-dev}"

if ! orb list | grep -q "^$machine "; then
    orb create rocky:9 "$machine"
fi

orb -m "$machine" bash -s <<'EOF'
set -eu
sudo dnf -y -q module enable nodejs:24 nginx:1.26 redis:7
sudo dnf -y -q install nodejs npm nginx redis rsync epel-release
sudo dnf -y -q install coturn coturn-utils
sudo tee /etc/yum.repos.d/mongodb-org-8.0.repo >/dev/null <<'REPO'
[mongodb-org-8.0]
name=MongoDB Repository
baseurl=https://repo.mongodb.org/yum/redhat/9/mongodb-org/8.0/$basearch/
gpgcheck=1
enabled=1
gpgkey=https://pgp.mongodb.com/server-8.0.asc
REPO
sudo dnf -y -q install mongodb-org
command -v pm2 >/dev/null || sudo npm install -g -s pm2
sudo systemctl enable --now mongod redis nginx
# PM2 跟着虚拟机开机启动（以当前用户运行）
sudo env PATH="$PATH" pm2 startup systemd -u "$(id -un)" --hp "$HOME" >/dev/null

# 中继（coturn，同时是 STUN）：3478 端口，UDP 和 TCP。凭据由服务端按共享密钥签发、有时效（use-auth-secret，
# Server/src/ice.ts）。共享密钥只在虚拟机上：第一次生成，之后不变；服务端经 TURN_SECRET_FILE 读它
secret=/etc/bilin/turn-secret
if ! sudo test -s "$secret"; then
    sudo mkdir -p /etc/bilin
    openssl rand -base64 32 | sudo tee "$secret" >/dev/null
fi
sudo chown "$(id -un):$(id -gn)" "$secret"
sudo chmod 600 "$secret"
# 听所有地址（0.0.0.0、::）：OrbStack 才把 3478 转到 Mac 的所有地址上，Mac 自己和局域网里的电脑都连得到
# （只听具体地址时只转到 Mac 的 localhost，docs/network.md）。
# 中继地址用虚拟机自己的地址：两端都经这台中继，中继之间的转发不出虚拟机
relay_ip=$(ip -4 -o addr show eth0 | awk '{print $4}' | cut -d/ -f1)
sudo tee /etc/coturn/turnserver.conf >/dev/null <<CONF
# 由 Server/deploy/setup-dev.sh 生成，要改就改它再跑一遍
listening-ip=0.0.0.0
listening-ip=::
listening-port=3478
no-tls
no-dtls
relay-ip=$relay_ip
min-port=49152
max-port=49407
use-auth-secret
static-auth-secret=$(cat "$secret")
realm=bilin
fingerprint
# 只在中继地址之间转发：不能被拿来当跳板去连别的机器
denied-peer-ip=0.0.0.0-255.255.255.255
denied-peer-ip=::-ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff
allowed-peer-ip=$relay_ip
no-multicast-peers
no-cli
log-file=/var/log/coturn/turnserver.log
simple-log
CONF
sudo chown root:coturn /etc/coturn/turnserver.conf
sudo chmod 640 /etc/coturn/turnserver.conf
sudo systemctl enable coturn
sudo systemctl restart coturn
node -v
echo "中继：$relay_ip，端口 3478"
EOF
