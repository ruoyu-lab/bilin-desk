import { fileURLToPath } from 'node:url'
import vue from '@vitejs/plugin-vue'
import { defineConfig } from 'vite'

// 管理后台的网页（docs/admin.md）：编出来是静态文件，Nginx 在 admin.bilindesk.com 上发（开发用的虚拟机在 /admin/ 下）；接口在同一个地址下的 api/，
// 由管理后台的进程（Server/src/admin）回答。本机开发时（npm run dev）把 /api 转给开发用的虚拟机
const here = (path: string) => fileURLToPath(new URL(path, import.meta.url))

export default defineConfig({
  plugins: [vue()],
  // 几个网站共用的图标、Logo（WebShared）：@shared/...；那里没有 node_modules，vue 从这里找
  resolve: { alias: { '@shared': here('../../WebShared') }, dedupe: ['vue'] },
  // 相对地址：放在 /admin/ 下面（开发用的虚拟机）也能找到脚本和样式
  base: './',
  build: { outDir: here('dist') },
  server: {
    // 网页图标用程序的 Logo（App/branding）
    fs: { allow: [here('.'), here('../../App/branding'), here('../../WebShared')] },
    proxy: {
      '/api': { target: 'http://localhost:8800', rewrite: (path) => `/admin${path}` },
    },
  },
})
