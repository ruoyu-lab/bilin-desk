# 管理后台

给软件的作者一个人用：看服务器在不在正常运行、备份数据库、发布新版本、看用户的反馈。不看任何一个账号、设备的内容，首页只有数目。

- 地址：`https://admin.bilindesk.com/`，自己一个域名，浏览器里和官网、网页版的数据互相碰不到
  （开发用的虚拟机只有一个地址：`http://localhost:8800/admin/`，PM2 的 `BASE_PATH`）
- 网页是 `Server/admin/`（Vue 3 + TypeScript），编出来是静态文件，Nginx 在根上发（`deploy/nginx/admin.bilindesk.com.conf`）；
  本机改网页时 `npm run dev`（在 Server/admin 里），接口转给开发用的虚拟机；Logo、图标用几个网站共用的 `WebShared/`
  （本站用到的图标列在 `src/icons.ts`）
- 接口（`/api/`，开发用的虚拟机上是 `/admin/api/`）是单独一个进程（PM2 的 `bilin-admin`，Server/src/admin/），只听本机的 3100 端口，Nginx 把请求转给它。
  和服务端用同一个 MongoDB、Redis

## 密码

只有一个密码，服务器上只存它的哈希（`/etc/bilin/admin-password`，只有服务端的用户能读）。部署过一次以后设：

```sh
Server/deploy/set-admin-password.sh        # 正式的服务器
Server/deploy/set-admin-password.sh dev    # 开发用的虚拟机
```

在这台电脑上输两遍，经 SSH 交给服务器。要改密码再跑一遍；登录时才读它，不用重启。

- 登录以后 12 小时有效（cookie 只在这个地址下、只经 HTTPS 发、网页里的脚本读不到）
- 试错：同一个地址 15 分钟最多 10 次，所有地址加起来 30 次，超了要等
- 改东西的请求（备份、发布、删除）只认这个网页自己发的

## 概览

只有数目，不看任何一个账号、设备的内容。30 秒刷新一次。

- 在线的设备、登记过的设备、账号的数目，数据库多大
- 证书还有几天到期（经 TLS 连本机 Nginx 看，少于 14 天标红）；上次备份是什么时候（超过 26 小时没有新的标红）；
  验证码邮件配没配好、今天发了几封、最近一次没发出去的原因
- 最近 7 天每天的数目：发起的连接（只算发起的一方）、同时在线最多几台（管理后台每分钟看一次）、新设备、新账号。
  连接、邮件的数目由服务端记在 Redis 里（`Server/src/stats.ts`，留 40 天），新设备、新账号从数据库里数；按服务器的本地日期分天
- 服务器的内存、磁盘、系统、CPU 负载、Node / MongoDB / Redis 的版本、运行了多久；各个服务（服务端、MongoDB、Redis、Nginx、coturn）
  在不在运行；最近 30 天上过线的设备按平台、程序的版本数（看新版本装上了多少）

## 用户反馈

用户在程序里提交的反馈（[feedback.md](feedback.md)），新的在前，没看过的前面有个点。点开一条看全文、设备 ID，记成看过了；
带了日志的点"日志"在新标签页里看（存的是 gzip 压缩的文本，浏览器自己解开）；也能删除。90 天后服务器自己删掉。

"通知邮箱"设上以后，有新的反馈时经 Resend 发一封邮件过去（内容、联系方式、设备和版本，不带日志），一天最多 20 封，
也算在概览里每天发的邮件数里；空着就不发。存在 MongoDB 的 `settings` 里，服务端每次收到反馈时读

## 出错日志

服务端、管理后台最近的警告和出错，最多 100 条，新的在前（`Server/src/admin/logs.ts`）：读 PM2 的日志文件
（`/opt/bilin/.pm2/logs`，每个文件只看末尾 256 KB），Fastify 写的 level 40 以上的，和进程崩了时往标准错误写的。
只显示说了什么、出错的原因，不显示日志里的设备 ID 这些。日志在服务器上只留 14 天。
"清空"是真的删：正在写的日志文件清成空的，轮换下来的旧日志删掉，平时的运行记录也一起没了，找不回来

## 发布新版本

1. 改 `CMakeLists.txt` 里的 `project(Bilin VERSION …)`，数字比现在发布的大（程序按它比新旧，`BILIN_VERSION_LABEL`
   的阶段 beta 不算），提交。发布出去的版本写成 `0.1.1-beta` 这样
2. 打安装包：
   - macOS：`scripts/build-macos-pkg.sh build/production build/release/<版本>`
   - Windows：`scripts/build-windows-vm.sh`，在 Mac 上的 Parallels 虚拟机里编（ARM64 的 Windows 也行，交叉编译成 x64），
     编好的 `Bilin-Setup.exe` 放在 `build/release/<版本>/`。在 Windows 上直接编：`scripts\build-windows-installer.cmd`，
     Qt 用 `msvc2022_64`，编好的复制到这台 Mac 上来签名
   - Android：`scripts/build-android.sh`，编好的 `build/android/Bilin.apk`（用 APK 的密钥库签名，[手机版](mobile.md)）
3. 签名：`node scripts/sign-release.mjs <安装包>`，旁边写出 `<安装包>.release.json`（版本号取 CMakeLists.txt 里的）
4. 发布：后台"版本发布"里把安装包和它的 `.release.json` 一起选上，点发布；或者 `scripts/publish-release.sh <安装包>`
   （经 SSH，服务器上做的和后台一样）

- **发布密钥**：私钥在这台 Mac 的 `~/.bilin/release-key.pem`（`node scripts/sign-release.mjs new-key` 生成的），
  不进仓库，**要备份**：丢了就发不了程序认的新版本，只能让大家手动装一个带新公钥的。公钥是 `Server/release-key.pub`，
  程序和服务器都用它
- 签的是"哪个平台、哪个版本、多大、sha256"这几行（Server/src/releases.ts、Core/src/rc/release.cpp）。后台上传时核对一遍，
  签名不对、不比现在发布的新的不收；程序下载以前、下载以后各核对一遍：服务器被人控制了也推不了假的安装包
- 现在发布的那个不能删（程序可能正在下载它）。要退回旧的，就用旧的代码改一个更大的版本号重新打包发布
- 安装包放在服务器的 `/var/lib/bilin/updates/<平台>/<版本>/`，程序和官网的下载按钮从 `https://www.bilindesk.com/updates/` 下载

程序怎么更新（Service/src/updater.h）：只有装好的（后台服务是系统服务）有新版本，开发时编出来的没有。
Android 版的见[手机版](mobile.md)的"更新"。

- 启动 2 分钟后问一次有没有新版本，之后每 6 小时一次。有了（签名核对过）就在左侧导航栏、登录的上面出现"更新到 x.x.x"
- 本机的人点了才下载（按钮上有进度），下载到数据目录（只有 SYSTEM / root 能进），核对签名、大小、sha256，再启动安装：
  Windows 由系统服务的进程启动 `Bilin-Setup.exe --quiet`（后台服务在看护者的作业里，它启动的进程会跟着它一起
  被结束）；macOS 交给 launchd 运行 `installer -pkg`（输出在数据目录的 `update-install.log`）
- 装的时候后台服务、界面程序都会被停掉、换掉；新的后台服务启动后把界面程序再打开，通知（铃铛）里记一条"程序更新到了 x.x.x"
- 没下载对、安装程序启动不了、装了却还是旧的版本（多半是打包时忘了改版本号）：按钮下面说上次没成，可以再点

## 备份

- mongodump 导出整个库（账号、设备登记），gzip 压缩成一个文件，放在服务器的 `/var/lib/bilin/backups`
- 每天凌晨 4 点（服务器的本地时间）自动一次，也可以点"立即备份"；只留最新的 14 份
- 想另外留一份时在后台里下载到自己的电脑上：服务器的盘坏了，留在服务器上的也一起没了

恢复（会先删掉现在库里的，换成备份里的）：

```sh
scp bilin-20261011-040000.archive.gz root@bilindesk.com:/tmp/
ssh root@bilindesk.com 'mongorestore --uri=mongodb://127.0.0.1:27017/bilin --gzip \
    --archive=/tmp/bilin-20261011-040000.archive.gz --drop && rm /tmp/bilin-20261011-040000.archive.gz'
```

Redis 里的（谁在线、限速的计数、登录的会话）不用备份：都是一会儿就过期的。

## 服务器的安全

`Server/deploy/harden-server.sh`（可以重复跑；服务器上原有的东西不动，防火墙只删、加规则，不重新加载）：

- SSH 只认密钥（`RC_SSH_PASSWORD=no`；不带时允许密码）：root 不能用密码登录，一次连接最多试 3 次，30 秒内不登录就断开，
  不转发 X11。云服务商控制台里的远程连接用的是 root 密码，不受影响。2026-10 时 7 天里有 10 万多次猜 root 密码
- fail2ban：10 分钟里 SSH 输错 5 次的地址封 1 小时，再犯封得更久（最多一周）
- 防火墙只开 SSH、80、443、中继（3478、49152-49407/udp），和服务器上原有的东西要用的端口；去掉了没在用的 cockpit、dhcpv6-client。
  云服务商控制台里的防火墙（安全组）也只放行这些
- 系统的安全更新每天自动装（dnf-automatic），不自动重启：内核的更新等下次重启
- MongoDB、Redis、服务端、管理后台都只听本机；服务端、后台以 bilin 用户运行，不用 root
- 443 上是服务器主人的 VPN（Xray，Reality），网站的 https 由它转给 Nginx 的 127.0.0.1:4443（docs/network.md）；
  部署时别动它的配置（`/usr/local/etc/xray/`）
- Nginx 不说版本号；管理后台的网页不能被别的网站嵌进去，只用自己的脚本和样式（Content-Security-Policy）

还可以做的（没做）：SELinux 现在是关着的，打开要重新标记整个文件系统、重启；MongoDB、Redis 没有密码（只听本机）。


## 邮件

注册、找回密码的验证码经 [Resend](https://resend.com) 发（`Server/src/mail.ts`，免费额度每月 3000 封、每天 100 封；服务端自己一天最多发 90 封）。
第一次配：

1. Resend 里 Domains › Add Domain：`bilindesk.com`，把它给的几条 DNS 记录（DKIM 的 TXT、`send` 子域名的 MX 和 SPF 的 TXT）
   加到 Cloudflare（Resend 可以授权 Cloudflare 自动加），等它显示 Verified
2. Resend 里 API Keys › Create：权限选 Sending access，域名选 bilindesk.com
3. 在这台电脑上运行 `Server/deploy/set-mail-key.sh`，输入上一步的密钥：存在服务器的 `/etc/bilin/mail-key`（只有服务端的用户能读），
   服务端随即重新读配置。密钥不进仓库；要换密钥再跑一遍

配好以后在后台概览的"验证码邮件"卡片里发一封测试邮件，看能不能收到（一小时最多 10 封，也算在每天的数目里）；
没发出去时那里直接显示 Resend 回的原因。发件人是 `比邻 <noreply@bilindesk.com>`（`Server/ecosystem.config.cjs` 的 `MAIL_FROM`）。没配密钥时服务端启动时警告，
要验证码的人看到"验证码邮件没发出去"；发不出去的原因在 `pm2 logs bilin-server` 里。

## 服务器上放了什么

装：`Server/deploy/setup-production.sh`（先要把域名指到服务器、云服务商的安全组放行端口，见脚本开头）；
部署：`Server/deploy/deploy-production.sh`（在这台电脑上编译，只传运行要的文件）。

| | 位置 |
| --- | --- |
| 服务端、管理后台的接口 | `/opt/bilin/server`（bilin 用户，PM2 的 `bilin-server` ×2 在 3000 端口、`bilin-admin` 在 3100 端口） |
| 官网的网页 | `/var/www/bilindesk.com` |
| 管理后台的网页 | `/var/www/admin.bilindesk.com` |
| 网页版 | `/var/www/web.bilindesk.com`（转发进程是 PM2 的 `bilin-relay`，3200 端口） |
| 发布的新版本、备份 | `/var/lib/bilin/updates`、`/var/lib/bilin/backups` |
| 中继的共享密钥、后台密码的哈希、发邮件的密钥 | `/etc/bilin` |
| 日志 | `/var/log/bilin`（Nginx 的出错日志、coturn），`/opt/bilin/.pm2/logs`；都只留 14 天 |
| Nginx | `/etc/nginx/conf.d/bilindesk.com.conf`、`admin.bilindesk.com.conf`、`web.bilindesk.com.conf`、`01-realip.conf`；不认识的域名、直接用 IP 来的由 `00-default.conf` 回答。443 是 Xray 的，网站都听 127.0.0.1:4443（docs/network.md） |
| 证书 | Let's Encrypt（`/etc/letsencrypt/live/bilindesk.com`），一张带着 bilindesk.com、www、admin、web，部署时缺了名字就补上；certbot 的定时任务自己续期，续好了重新加载 Nginx |

同一台服务器以后再放别的网站：程序放在它自己的目录、用它自己的用户和端口（不要用 3000、3100），
Nginx 在 `conf.d` 里加一份按它的域名起名的配置（https 听 127.0.0.1:4443、带 `proxy_protocol`）；新域名先 `certbot certonly --webroot -w /var/www/letsencrypt -d 域名`
申请证书（验证请求由 `00-default.conf` 回答），再装它的配置。
