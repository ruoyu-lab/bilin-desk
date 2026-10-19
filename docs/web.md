# 网页版

在 Chrome、Edge 里控制装了比邻的电脑，地址 `web.bilindesk.com`。网页只能去连别人，不能被连。

- 画面最高 1080p、每秒 30 帧，经服务器转发（浏览器不能直连，也不能用 TURN 中继）。服务器的带宽是共用的 200 Mbit/s，
  更清晰流畅的画面用客户端
- 只支持 Chromium 内核的浏览器（Chrome、Edge）：SDK 的浏览器包只在它们上面验证过；别的浏览器打开时说"请用 Chrome 或 Edge 打开"
- 自己一个域名：浏览器按域名隔开数据，官网、管理后台的脚本碰不到网页版存的设备令牌、设备密钥
- SDK 的浏览器包要跨源隔离（`SharedArrayBuffer`）：网页的响应带 `Cross-Origin-Opener-Policy: same-origin` 和
  `Cross-Origin-Embedder-Policy: require-corp`（本机开发时 `vite.config.ts` 也带上），还要严格的内容安全策略，
  不放任何第三方脚本（SDK 的 docs/guide/23-browser.md）

## 现在做到哪里

界面先用假数据做（`Web/src/state.ts`），定稿以后再接服务器（登录、我的设备）和 SDK 的浏览器包（画面、键鼠、剪贴板、文件）。

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

## 开发

```sh
cd Web && npm install && npm run dev
```

Vue 3 + TypeScript + Vite，和官网、管理后台一样，各自是独立的项目；图标、Logo 用三个网站共用的 `WebShared/`
（本站用到的图标列在 `src/icons.ts`）。颜色照程序的 `App/qml/common/Theme.qml`。
