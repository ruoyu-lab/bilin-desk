# 网页版

在 Chrome、Edge 里控制装了比邻的电脑，地址 `web.bilindesk.com`。网页只能去连别人，不能被连。

- 画面最高 1080p、每秒 30 帧，经服务器转发（浏览器不能直连，也不能用 TURN 中继）。服务器的带宽是共用的 200 Mbit/s，
  更清晰流畅的画面用客户端
- 只支持 Chromium 内核的浏览器（Chrome、Edge）：SDK 的浏览器包只在它们上面验证过；别的浏览器打开时说"请用 Chrome 或 Edge 打开"
- 自己一个域名：浏览器按域名隔开数据，官网、管理后台的脚本碰不到网页版存的设备令牌、设备密钥
- SDK 的浏览器包要跨源隔离（`SharedArrayBuffer`）：网页的响应带 `Cross-Origin-Opener-Policy: same-origin` 和
  `Cross-Origin-Embedder-Policy: require-corp`（本机开发时 `vite.config.ts` 也带上），还要严格的内容安全策略，
  不放任何第三方脚本（SDK 的 docs/guide/23-browser.md）

## 怎么连

浏览器不能直连，也不能用 coturn：经服务器上的 Node 转发进程（`Server/src/relay/`，PM2 的 `bilin-relay`），
走 SDK 的宿主通道（`RCS_TRANSPORT_HOST`，数据报）。加密、重传、码率、重连都是两端的 SDK 端到端做的，转发进程只按房间
把二进制消息原样转过去，看不懂内容。

1. 浏览器 `POST /web/connect {deviceId}`（服务端，`Server/src/server.ts`）：服务端给那台电脑、这个浏览器各发一张
   进它房间的一次性通行证（`Server/src/tickets.ts`，Redis 里，一分钟内有效、用一次就作废），经长连接告诉电脑
   （`webViewer`）。同一个地址一小时最多 60 次。不用登录：连上以后还要输对方的临时密码，对方还要点允许
2. 电脑（后台服务的 `ShareHost`）：别人的那个分享会话换成"经网页版的转发"（`rc::WebTransport`，和直连、中继一样
   一次运行一种，规则见 `ShareTransport`：正被别人用另一种方式连着时这次不行），凭通行证连
   `ws(s)://服务器/relay`（`Service/src/relay_channel.h`）。人都走了换回直连
3. 浏览器（`Web/src/remote.ts`、`SessionPage.vue`）：SDK 的浏览器包 `connectWebSocket` 到同一个房间
   （`channel: 'datagram'`：一条消息一个数据报），`identity: { pin }` 是对方的临时密码；画面 `VideoView`、
   键鼠 `InputMapper`、对方的指针 `CursorView`、剪贴板 `ClipboardSync`、发文件。要 1080p、30 帧
4. 转发进程（`Server/src/relay/server.ts`）：一个房间一台电脑、最多 8 个浏览器。电脑那条连接上每条消息前面带
   8 字节的路由号（转发进程给每个浏览器的编号，就是 SDK 的 route），空的数据报是"这个浏览器走了 / 不要它了"；
   对端收不过来时丢掉，SDK 重传、降码率

电脑上来的人显示成"网页版"（没有设备 ID），平台 Web。

SDK 的浏览器包：`scripts/build-web-sdk.sh` 用 Emscripten（`~/emsdk`）编在 `build/rcsdk-wasm`，拷到
`Web/public/sdk/`（不进仓库，wasm 约 2.7 MB）。

## 现在做到哪里

- 连接打通了（本机经开发用的虚拟机）：要通行证、通知电脑、转发进程配对、浏览器的 SDK 经它发握手都试过（假的被控端）；
  真的电脑那一端要用新编的开发版试
- 设备列表、登录还是假数据（`Web/src/state.ts`）；发送快捷键的菜单还没接上

- 概览：按设备 ID 和临时密码连接；没登录时请登录，登录了说说客户端；我的设备、最近连过的
- 我的设备、收藏、最近：一行一台，收藏、连接
- 会话在新的标签页里（几台可以一起连），断开就关掉那个标签页。临时密码不放进地址：打开前以一个随机的交接编号
  存进浏览器，新的标签页拿到就删掉，地址里只有编号；刷新过、直接打开地址时在会话页里再输一次。浏览器拦了新窗口时
  就在原来的标签页里打开
- 会话：上面一条细的工具栏（发送快捷键、剪贴板同步、发送文件、全屏、断开），下面铺满对方的画面；正在连接的样子，
  画质的提示
- 设置：账号、外观（跟随系统、浅色、深色，记在浏览器里）、关于网页版
- 页面跟着地址的 #：`#/`、`#/devices`、`#/favorites`、`#/recent`、`#/settings`、`#/session/<设备 ID>/<交接编号>`；本机开发时
  `#/unsupported` 看"请用 Chrome 或 Edge 打开"那一页

## 部署

`Server/deploy/deploy-production.sh` 一起部署：编 SDK 的浏览器包和网页，放进 `/var/www/web.bilindesk.com`，
Nginx 的配置 `Server/deploy/nginx/web.bilindesk.com.conf`（跨源隔离、严格的内容安全策略；`/sdk/` 补上 `.mjs` 的类型，
系统的 MIME 表里没有；`/web/connect` 给服务端，`/relay` 给转发进程），转发进程是 PM2 的 `bilin-relay`（127.0.0.1:3200）。

## 开发

```sh
scripts/build-web-sdk.sh            # SDK 的浏览器包（第一次、SDK 换了版本时）
cd Web && npm install && npm run dev
```

本机开发时 `/web/connect`、`/relay` 转给开发用的虚拟机（`vite.config.ts` 的 `proxy`，`Server/deploy/deploy-dev.sh`）。

Vue 3 + TypeScript + Vite，和官网、管理后台一样，各自是独立的项目；图标、Logo 用三个网站共用的 `WebShared/`
（本站用到的图标列在 `src/icons.ts`）。颜色照程序的 `App/qml/common/Theme.qml`。
