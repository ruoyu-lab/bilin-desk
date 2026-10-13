import { fileURLToPath } from 'node:url'
import vue from '@vitejs/plugin-vue'
import { defineConfig } from 'vite'

// 管理后台的网页（docs/admin.md）：编出来是静态文件，Nginx 在 /admin/ 下发；接口在同一个地址下的 api/，
// 由管理后台的进程（Server/src/admin）回答。本机开发时（npm run dev）把 /api 转给开发用的虚拟机
export default defineConfig({
  plugins: [vue()],
  // 相对地址：放在 /admin/ 下面也能找到脚本和样式
  base: './',
  build: { outDir: fileURLToPath(new URL('dist', import.meta.url)) },
  server: {
    // 网页图标用程序的 Logo（App/branding）
    fs: { allow: [fileURLToPath(new URL('.', import.meta.url)), fileURLToPath(new URL('../../App/branding', import.meta.url))] },
    proxy: {
      '/api': { target: 'http://localhost:8800', rewrite: (path) => `/admin${path}` },
    },
  },
})
