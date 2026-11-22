#!/bin/sh
# 设发验证码邮件用的 Resend API 密钥（docs/admin.md 的"邮件"）：在这台电脑上输入，经 SSH 交给服务器，
# 存在 /srv/bilin/secrets/mail-key（只有比邻的容器能读），再一个一个重启服务端（启动时读）。密钥不出现在命令行里、不进仓库。
# 密钥在 Resend 里建：权限选"只能发送"（Sending access），域名选 bilindesk.com
#
#   Server/deploy/set-mail-key.sh
set -eu

trap 'stty echo 2>/dev/null' EXIT INT TERM
printf 'Resend 的 API 密钥（re_ 开头）：'
stty -echo
read -r key
stty echo
echo
case "$key" in
    re_*) ;;
    *) echo '不像 Resend 的密钥（应该以 re_ 开头），没改'; exit 1 ;;
esac

# 密钥经标准输入给，不出现在命令行里（别的用户看得到命令行）
printf '%s' "$key" | ssh "${RC_HOST:-ubuntu@bilindesk.com}" '
set -eu
cd /srv/bilin
# 原地写（挂进容器的是这个文件本身，换成新文件容器里看不到）
sudo sh -c "cat > secrets/mail-key"
for service in server-1 server-2; do
    sudo docker compose restart "$service" >/dev/null
    for _ in $(seq 30); do
        [ "$(sudo docker inspect -f "{{.State.Health.Status}}" "$(sudo docker compose ps -q "$service")")" = healthy ] && break
        sleep 2
    done
done
echo "存好了，服务端已经重新读了配置"
'
