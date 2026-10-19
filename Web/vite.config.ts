import { fileURLToPath } from 'node:url'
import vue from '@vitejs/plugin-vue'
import { defineConfig } from 'vite'

// 网页版（docs/web.md）：编出来是静态文件，Nginx 在 web.bilindesk.com 上发。SDK 的浏览器包要跨源隔离
// （SharedArrayBuffer），本机看时也带上这两个头，和正式服务器一样
const isolation = {
  'Cross-Origin-Opener-Policy': 'same-origin',
  'Cross-Origin-Embedder-Policy': 'require-corp',
}

export default defineConfig({
  plugins: [vue()],
  server: {
    headers: isolation,
    // Logo 用程序的（App/branding）
    fs: { allow: [fileURLToPath(new URL('.', import.meta.url)), fileURLToPath(new URL('../App/branding', import.meta.url))] },
  },
  preview: { headers: isolation },
})
