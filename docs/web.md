# 网页版

在 Chrome、Edge 里控制装了比邻的电脑，地址 `web.bilindesk.com`。网页只能去连别人，不能被连。

- 画面最高 1080p、每秒 30 帧，经服务器转发（浏览器不能直连，也不能用 TURN 中继）。服务器的带宽是共用的 200 Mbit/s，
  更清晰流畅的画面用客户端
- 只支持 Chromium 内核的浏览器（Chrome、Edge）：SDK 的浏览器包只在它们上面验证过；别的浏览器打开时说"请用 Chrome 或 Edge 打开"
- 自己一个域名：浏览器按域名隔开数据，官网、管理后台的脚本碰不到网页版的登录、连接时的临时密码和钥匙
- SDK 的浏览器包要跨源隔离（`SharedArrayBuffer`）：网页的响应带 `Cross-Origin-Opener-Policy: same-origin` 和
  `Cross-Origin-Embedder-Policy: require-corp`（本机开发时 `vite.config.ts` 也带上），还要严格的内容安全策略，
  不放任何第三方脚本（SDK 的 docs/guide/23-browser.md）

## 怎么连

浏览器不能直连，也不能用 coturn：经服务器上的 Node 转发进程（`Server/src/relay/`，PM2 的 `bilin-relay`），
走 SDK 的宿主通道（`RCS_TRANSPORT_HOST`，数据报）。加密、重传、码率、重连都是两端的 SDK 端到端做的，转发进程只按房间
把二进制消息原样转过去，看不懂内容。

1. 浏览器 `POST /web/connect {deviceId}`（服务端，`Server/src/web.ts`）：
   - 转发进程报的路数满了（`relay-load.ts`）：说人太多（`busy`）。同一个地址一小时最多 60 次
   - 服务端给那台电脑、这个浏览器各发一张进它房间的一次性通行证（`tickets.ts`，Redis 里，一分钟内有效、用一次就作废），
     经长连接告诉电脑（`webViewer`）
   - 浏览器登录了那台电脑的账号（我的设备）：再给一把这次连接用的一次性钥匙（32 字节随机数），房间是 `<ID>/mine`；
     别人的电脑没有钥匙，房间是 `<ID>`
   - 等电脑回答（`webReady`，最多 8 秒）：准备好了（进了房间、换好了钥匙）才把浏览器的那张交出去；电脑说不行时说原因
     （没开"允许别人连接本机"、正被别人用另一种方式连着、没授权录屏……）；一直不回答（旧版本）说对方的比邻可能不是最新版本
2. 电脑（后台服务，`Sharing`）：
   - 别人的（临时密码）交给"别人"那个分享会话：换成"经网页版的转发"（`rc::WebTransport`，和直连、中继一样一次运行一种，
     规则见 `ShareTransport`：正被别人用另一种方式连着时这次不行），凭通行证连 `ws(s)://服务器/relay`
     （`Service/src/relay_channel.h`）。浏览器要输临时密码，本机的人点允许。人都走了换回直连
   - 我的设备（钥匙）交给第三个分享会话"网页版的我的设备"（`rc::WebKeyAccess`）：SDK 一次只认一种口令，所以单独一个，
     认这把钥匙（SDK 的 KEY），不要密码、不用点允许；只为浏览器开着，钥匙每次换，人走了就停
   - 网页版的人路断了算走了：浏览器再连要新的通行证，不会从原路回来
3. 浏览器（`Web/src/remote.ts`、`SessionPage.vue`）：SDK 的浏览器包 `connectWebSocket` 到同一个房间
   （`channel: 'datagram'`：一条消息一个数据报），`identity` 是钥匙或者对方的临时密码；画面 `VideoView`、
   键鼠 `InputMapper`、对方的指针 `CursorView`、剪贴板 `ClipboardSync`、文件两个方向。要 1080p、30 帧、6 Mbit/s
4. 转发进程（`Server/src/relay/server.ts`）：一个房间一台电脑、最多 8 个浏览器，所有房间加起来最多 `WEB_MAX_SESSIONS`
   （20）个，发给一个浏览器的超过 `WEB_MAX_VIEWER_BPS`（8 Mbit/s）的丢掉。电脑那条连接上每条消息前面带
   8 字节的路由号（转发进程给每个浏览器的编号，就是 SDK 的 route），空的数据报是"这个浏览器走了 / 不要它了"；
   对端收不过来时丢掉，SDK 重传、降码率

电脑上来的人显示成"网页版"（没有设备 ID），平台 Web。

SDK 的浏览器包：`scripts/build-web-sdk.sh` 用 Emscripten（`~/emsdk`）编在 `build/rcsdk-wasm`，拷到
`Web/public/sdk/`（不进仓库，wasm 约 2.7 MB）。

## 账号

网页版自己的接口（`Server/src/web.ts`，`/web/` 下）：`login`、`logout`、`code`（发验证码）、`register`、`reset`
（找回密码）、`me`（登录着的账号和它下面的设备、在不在线）。规则和客户端一样（输错的密码按地址、按邮箱数，验证码一分钟一个）。
登录以后是一个随机令牌的 cookie（`bilin_web`，`Path=/web`、HttpOnly、Secure、SameSite=Strict，30 天没用就作废，
`web-sessions.ts`）；改东西的请求还核对 Origin。浏览器不登记成设备：账号下的设备列表里没有它。

## 网页

- 概览：按设备 ID 和临时密码连接（输的是我的设备时不要密码）；没登录时请登录，登录了说说客户端；我的设备、最近连过的
- 我的设备（服务器说的，只列 Mac、Windows：手机、平板不能被连）、收藏、最近（记在浏览器里：`bilin.devices`，几个标签页共用）
- 不在线的我的设备能叫醒的（它告诉过服务器网卡，账号下有开着的别的电脑）："连接"换成"唤醒"（概览的卡片点一下就叫），
  浏览器发不了唤醒包，请账号下开着的电脑帮忙（`POST /web/wake`，docs/devices-and-accounts.md 的"远程唤醒"）；
  叫得怎么样写在设备 ID 后面，发出去以后隔 5 秒问一次它上线没有
- 会话在新的标签页里（几台可以一起连），断开就关掉那个标签页（先和对方告别）。临时密码不放进地址：打开前以一个随机的
  交接编号存进浏览器，新的标签页拿到就删掉，地址里只有编号；刷新过、直接打开地址时在会话页里再输一次。浏览器拦了新窗口时
  就在原来的标签页里打开
- 会话页：上面一条细的工具栏（对方的系统、名字，经服务器转发、多大、几帧、往返多久；换显示器（几块时）、发送快捷键、
  剪贴板同步的开关、发送文件（也可以拖到画面上）、全屏、断开），下面铺满对方的画面；对方发来的文件问收不收，收下存进下载；
  全屏时 Chrome 把 Esc 这些键也交给页面（Keyboard Lock），按住 Esc 退出；连着时关标签页先问一下
- 设置：账号、外观（跟随系统、浅色、深色，记在浏览器里）、关于网页版
- 页面跟着地址的 #：`#/`、`#/devices`、`#/favorites`、`#/recent`、`#/settings`、`#/session/<设备 ID>/<交接编号>`；本机开发时
  `#/unsupported` 看"请用 Chrome 或 Edge 打开"那一页

## 测试过的（2026-10-19，本机经开发用的虚拟机，Chromium）

用单独数据目录（`RC_STATE_DIR`）开的开发版当被控的电脑（Mac，两块显示器），一个小脚本经本机连接替人点允许、收发文件：
临时密码连（请求、允许、画面 1920×1080 30 帧、往返 2～4 ms）、刷新以后再输密码、换显示器、键鼠（真的鼠标跟着动）、
电脑发给网页的文件（内容一致、触发下载）、网页拖进去的文件（电脑收到、内容一致）、断开（对方收到告别、换回直连）、
注册（验证码）、登录（输错密码的提示）、退出、我的设备（不要密码、不用点允许，断开后那个分享会话停掉）、收藏、
码率按 6 Mbit/s。没在真机上按发送快捷键（会动到那台 Mac），也没测剪贴板（要浏览器问剪贴板权限）。

## 部署

`Server/deploy/deploy-production.sh` 一起部署：编 SDK 的浏览器包和网页，放进 `/var/www/web.bilindesk.com`，
Nginx 的配置 `Server/deploy/nginx/web.bilindesk.com.conf`（跨源隔离、严格的内容安全策略；`/sdk/` 补上 `.mjs` 的类型，
系统的 MIME 表里没有；`/web/` 给服务端，`/relay` 给转发进程），转发进程是 PM2 的 `bilin-relay`（127.0.0.1:3200）。

## 开发

```sh
scripts/build-web-sdk.sh            # SDK 的浏览器包（第一次、SDK 换了版本时）
cd Web && npm install && npm run dev
```

本机开发时 `/web/`、`/relay` 转给开发用的虚拟机（`vite.config.ts` 的 `proxy`，带着原来的 Host，`Server/deploy/deploy-dev.sh`）。

Vue 3 + TypeScript + Vite，和官网、管理后台一样，各自是独立的项目；图标、Logo 用三个网站共用的 `WebShared/`
（本站用到的图标列在 `src/icons.ts`）。颜色照程序的 `App/qml/common/Theme.qml`。
