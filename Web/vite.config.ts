import { fileURLToPath } from 'node:url'
import vue from '@vitejs/plugin-vue'
import { defineConfig } from 'vite'

// 网页版（docs/web.md）：编出来是静态文件，Nginx 在 web.bilindesk.com 上发。SDK 的浏览器包要跨源隔离
// （SharedArrayBuffer），本机看时也带上这两个头，和正式服务器一样
const isolation = {
  'Cross-Origin-Opener-Policy': 'same-origin',
  'Cross-Origin-Embedder-Policy': 'require-corp',
}

const here = (path: string) => fileURLToPath(new URL(path, import.meta.url))

export default defineConfig({
  plugins: [vue()],
  // 几个网站共用的图标、Logo（WebShared）：@shared/...；那里没有 node_modules，vue 从这里找
  resolve: { alias: { '@shared': here('../WebShared') }, dedupe: ['vue'] },
  server: {
    headers: isolation,
    // 网页图标用程序的 Logo（App/branding）
    fs: { allow: [here('.'), here('../App/branding'), here('../WebShared')] },
  },
  preview: { headers: isolation },
})
