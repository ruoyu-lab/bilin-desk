# 官网

`Website/`：Vue 3 + TypeScript，Vite 构建成静态网页（首页、服务协议、隐私政策三页），放在 https://www.bilindesk.com/
（`Server/deploy/deploy-production.sh` 一起部署，服务器上的 `/var/www/bilindesk.com`）。

```sh
cd Website
npm install
npm run dev       # 本机看：http://localhost:5173/
npm run build     # 类型检查，再构建到 Website/dist/
npm run preview   # 看构建出来的
```

- 样子和程序一样：白底黑字、没有强调色、卡片用填充色（`src/styles.css` 的颜色取自程序的 Theme.qml 浅色）
- 不用网络字体、不接第三方统计：国内打不开谷歌字体，也不想把访客交给别人（隐私政策说了不含第三方）
- 服务协议、隐私政策直接读程序里的那两份（`App/legal/*.md`），只改那里；用到的 Markdown 只有标题、段落、列表、加粗
  （`src/legal/markdown.ts`），文档里用了别的写法要在那里加上
- Logo 用程序的 `App/branding/logo.svg`
- 下载按钮问服务器现在发布的版本（`/updates/<平台>/latest`，和程序检查更新同一个，见 [admin.md](admin.md)），
  直接指向安装包；网页和服务器在同一个域名下。本机看网页时这些请求转给正式服务器（`vite.config.ts`）。还没发布的平台说"还没有发布"，连不上服务器时说"暂时取不到安装包"
- 页头的"赞助"（`src/components/SponsorDialog.vue`）：浮窗里写赞助用在哪（服务器、Apple 开发者账号、各平台的客户端）
  和微信收款码（`src/assets/wechat-pay.jpg`，从收款码图片裁出中间的白色卡片，二维码没动）
- 构建出来的地址都是相对的，放在服务器的哪个目录下都行
- TypeScript 用 6：vue-tsc 还不支持 7（7 是原生重写的，没有它要用的接口）
