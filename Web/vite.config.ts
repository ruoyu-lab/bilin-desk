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
    // 本机开发时，网页版的接口、转发进程交给开发用的虚拟机（Server/deploy/deploy-dev.sh）。带着原来的 Host：
    // 服务端核对改东西的请求的 Origin（简写的代理会把它换成虚拟机的）
    proxy: {
      '/web': { target: 'http://localhost:8800', changeOrigin: false },
      '/relay': { target: 'ws://localhost:8800', ws: true, changeOrigin: false },
    },
    // 网页图标用程序的 Logo（App/branding）
    fs: { allow: [here('.'), here('../App/branding'), here('../WebShared')] },
  },
  preview: { headers: isolation },
})
