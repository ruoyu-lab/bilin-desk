import { fileURLToPath } from 'node:url'
import vue from '@vitejs/plugin-vue'
import { defineConfig, type ProxyOptions } from 'vite'

const here = (path: string) => fileURLToPath(new URL(path, import.meta.url))

// 网页和正式服务器放在一起（bilindesk.com）：下载按钮问 /updates 现在发布的是哪个版本（程序更新也问它）。
// 本机看网页时（dev、preview）把这些请求转给正式服务器
const updates: Record<string, ProxyOptions> = {
  '/updates': { target: 'https://www.bilindesk.com', changeOrigin: true },
}

// 两次构建（package.json 的 build）：先是浏览器里的三页；再是 --ssr src/server.ts，给 scripts/prerender.mjs 在 Node 里
// 把三页渲染成 HTML 填进去
export default defineConfig(({ isSsrBuild }) => ({
  plugins: [vue()],
  // 相对地址：放在服务器的哪个目录下都能打开
  base: './',
  build: isSsrBuild
    ? { outDir: 'dist-ssr' }
    : {
        rolldownOptions: {
          input: {
            index: here('index.html'),
            privacy: here('privacy.html'),
            terms: here('terms.html'),
          },
        },
      },
  server: {
    proxy: updates,
    // 服务协议、隐私政策和 Logo 直接用程序里的那几份（App/legal、App/branding）
    fs: { allow: [here('.'), here('../App/legal'), here('../App/branding')] },
  },
  preview: { proxy: updates },
}))
