#!/bin/sh
# 设管理后台的密码（docs/admin.md）：在这台电脑上输两遍，经 SSH 交给服务器，服务器上只存它的哈希。
# 先部署过一次（set-password.js 在部署编出来的 dist 里）
#
#   Server/deploy/set-admin-password.sh          正式的服务器
#   Server/deploy/set-admin-password.sh dev      开发用的虚拟机
set -eu

trap 'stty echo 2>/dev/null' EXIT INT TERM
printf '管理后台的新密码（至少 12 个字）：'
stty -echo
read -r password
stty echo
echo
printf '再输一遍：'
stty -echo
read -r again
stty echo
echo
if [ "$password" != "$again" ]; then
    echo '两遍不一样，没改'
    exit 1
fi

file=/etc/bilin/admin-password
script=/opt/bilin/server/dist/admin/set-password.js
# 密码经标准输入给，不出现在命令行里（别的用户看得到命令行）
if [ "${1:-}" = dev ]; then
    printf '%s' "$password" | orb -m "${RC_MACHINE:-bilin-dev}" bash -c \
        "sudo node $script $file && sudo chown \"\$(id -u):\$(id -g)\" $file"
else
    printf '%s' "$password" | ssh "${RC_HOST:-root@bilindesk.com}" \
        "node $script $file && chown bilin:bilin $file"
fi
