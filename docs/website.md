# 官网

`Website/`：Vue 3 + TypeScript，Vite 构建成静态网页（首页、服务协议、隐私政策三页），放在 https://www.bilindesk.com/
（`Server/deploy/deploy-production.sh` 一起部署，服务器上的 `/var/www/bilindesk.com`）。

```sh
cd Website
npm install
npm run dev       # 本机看：http://localhost:5173/
npm run build     # 类型检查，构建到 Website/dist/，再预渲染（见下）
npm run preview   # 看构建出来的
```

- 样子和程序一样：白底黑字、没有强调色、卡片用填充色（`src/styles.css` 的颜色取自程序的 Theme.qml 浅色）
- 不用网络字体、不接第三方统计：国内打不开谷歌字体，也不想把访客交给别人（隐私政策说了不含第三方）
- 服务协议、隐私政策直接读程序里的那两份（`App/legal/*.md`），只改那里；用到的 Markdown 只有标题、段落、列表、加粗
  （`src/legal/markdown.ts`），文档里用了别的写法要在那里加上
- Logo 用程序的 `App/branding/logo.svg`；页面里的 Logo、图标用几个网站共用的 `WebShared/`（本站用到的图标列在 `src/icons.ts`）
- 下载按钮问服务器现在发布的版本（`/updates/<平台>/latest`，和程序检查更新同一个，见 [admin.md](admin.md)），
  直接指向安装包；网页和服务器在同一个域名下。本机看网页时这些请求转给正式服务器（`vite.config.ts`）。还没发布的平台说"还没有发布"，连不上服务器时说"暂时取不到安装包"
- 页头的"赞助"（`src/components/SponsorDialog.vue`）：浮窗里写赞助用在哪（服务器、Apple 开发者账号、各平台的客户端）
  和微信收款码（`src/assets/wechat-pay.jpg`，从收款码图片裁出中间的白色卡片，二维码没动）
- 构建出来的地址都是相对的，放在服务器的哪个目录下都行
- TypeScript 用 6：vue-tsc 还不支持 7（7 是原生重写的，没有它要用的接口）

## 搜索引擎（只做了 Google，Search Console 用户自己管）

- **预渲染**：`npm run build` 先构建浏览器里的三页，再 `vite build --ssr src/server.ts` 编一份在 Node 里渲染的，
  `scripts/prerender.mjs` 用它把三页渲染成完整的 HTML 填进 `dist`（`<div id="app">`），浏览器里 `createSSRApp` 接管。
  搜索引擎、分享卡片不用跑脚本就读得到页面上的字。所以组件在建立时（setup、渲染）不能碰浏览器才有的东西（`window`、
  `document`、`fetch`），放到 `onMounted` 里；也不能按时间、随机数渲染（Node 里和浏览器里渲染得要一样，不然接管时对不上）
- 挪到页面末尾的东西（赞助的浮窗，`Teleport`）挪进 `#teleports`，不直接挪到 `body`：接管时 Vue 认为挪过去的地方开头就是它的标记
- 每页的 `<head>` 里有 description、canonical、Open Graph / Twitter 卡片（`public/og.png`）、PNG 图标；首页的结构化数据
  （SoftwareApplication 和常见问题的 FAQPage，常见问题在 `src/faq.ts`，页面上显示的是同一份）由 `prerender.mjs` 写进去
- `public/robots.txt`（不让爬 `/updates/`；管理后台在自己的域名上，响应头里说不让收）；`sitemap.xml` 由 `prerender.mjs` 每次构建写，日期是构建那天。
  加了新页面：`vite.config.ts` 的 input、`server.ts` 的 render、`prerender.mjs` 的 pages 一起加
- `public/` 里的 og.png、favicon-32.png、apple-touch-icon.png 由 `scripts/make-icons.sh` 从 Logo 生成
