#!/bin/sh
# 正式服务器的安全加固（docs/admin.md 的"服务器的安全"）。可以重复跑；服务器上原有的东西不动，
# 防火墙只删、加规则，不重新加载：
# - SSH：不转发 X11，一次连接最多试 3 次密码，30 秒内不登录就断开。RC_SSH_PASSWORD=no 时只认密钥（先确认自己的
#   密钥能登录；云服务商控制台里的远程连接用的是 root 密码，不受影响）
# - fail2ban：10 分钟里输错 5 次 SSH 密码的地址封 1 小时，再犯封得更久
# - 防火墙：去掉没在用的 cockpit（网页管理，9090 端口）
# - 系统的安全更新每天自动装，不自动重启
#
#   Server/deploy/harden-server.sh
#   RC_SSH_PASSWORD=no Server/deploy/harden-server.sh
set -eu

host="${RC_HOST:-root@bilindesk.com}"
password="${RC_SSH_PASSWORD:-yes}"

ssh "$host" bash -s -- "$password" <<'EOF'
set -eu
password="$1"

# SSH：放在 sshd_config.d 里最前面（同一个设置以先读到的为准）
cat > /etc/ssh/sshd_config.d/00-hardening.conf <<CONF
# 由 Server/deploy/harden-server.sh 生成，要改就改它再跑一遍
X11Forwarding no
MaxAuthTries 3
LoginGraceTime 30
PasswordAuthentication $password
PermitRootLogin $( [ "$password" = no ] && echo prohibit-password || echo yes )
CONF
chmod 600 /etc/ssh/sshd_config.d/00-hardening.conf
sshd -t
systemctl reload sshd

# 防火墙：cockpit 没在用。先于 fail2ban：装它时防火墙会按存下的规则重新加载一次
if firewall-cmd -q --permanent --query-service=cockpit; then
    firewall-cmd -q --permanent --remove-service=cockpit
fi
if firewall-cmd -q --query-service=cockpit; then
    firewall-cmd -q --remove-service=cockpit
fi

# fail2ban：封的是防火墙里临时的规则，不重新加载防火墙
dnf -y -q install fail2ban fail2ban-firewalld
cat > /etc/fail2ban/jail.d/sshd.local <<'CONF'
# 由 Server/deploy/harden-server.sh 生成
[sshd]
enabled = true
backend = systemd
maxretry = 5
findtime = 10m
bantime = 1h
bantime.increment = true
bantime.maxtime = 1w
CONF
systemctl enable -q --now fail2ban
systemctl restart fail2ban

# 系统的安全更新：每天自动装（不自动重启：内核的更新等下次重启，要重启时自己挑时间）
dnf -y -q install dnf-automatic
sed -i -e 's/^upgrade_type *=.*/upgrade_type = security/' -e 's/^apply_updates *=.*/apply_updates = yes/' \
    -e 's/^reboot *=.*/reboot = never/' /etc/dnf/automatic.conf
systemctl enable -q --now dnf-automatic.timer

sleep 2
fail2ban-client status sshd | sed -n '1,4p'
sshd -T | grep -E '^(passwordauthentication|permitrootlogin|maxauthtries|x11forwarding) '
firewall-cmd --list-services
EOF
