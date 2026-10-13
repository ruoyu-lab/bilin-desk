#!/bin/sh
# 设发验证码邮件用的 Resend API 密钥（docs/admin.md 的"邮件"）：在这台电脑上输入，经 SSH 交给服务器，
# 存在 /etc/bilin/mail-key（只有服务端的用户能读），再让服务端重新读配置。密钥不出现在命令行里、不进仓库。
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
printf '%s' "$key" | ssh "${RC_HOST:-root@bilindesk.com}" '
set -eu
umask 077
cat > /etc/bilin/mail-key
chown bilin:bilin /etc/bilin/mail-key
chmod 600 /etc/bilin/mail-key

runuser -u bilin -- env HOME=/opt/bilin PATH="$PATH" pm2 reload bilin-server >/dev/null
echo "存好了，服务端已经重新读了配置"
'
