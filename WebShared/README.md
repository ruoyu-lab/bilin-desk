# WebShared

几个网站共用的部分：官网（`Website/`，www.bilindesk.com）、管理后台（`Server/admin/`，admin.bilindesk.com）、
网页版（`Web/`，web.bilindesk.com）。每个网站是自己的 Vue 项目，只拿用得到的：

- `icons.ts`：线条图标，每个单独导出；网站在自己的 `src/icons.ts` 里列出用到的那几个，打包时别的不带
- `LineIcon.vue`：画一个图标（网站的 `components/LineIcon.vue` 按名字取了交给它）
- `LogoMark.vue`：程序的 Logo

网站里用 `@shared/...` 引用（`vite.config.ts` 的 `resolve.alias`、`tsconfig.json` 的 `paths`）。这里没有自己的
`node_modules`：`vue` 从引用它的网站里找（`resolve.dedupe`）。
