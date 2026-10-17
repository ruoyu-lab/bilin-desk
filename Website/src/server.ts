import { createSSRApp } from 'vue'
import { renderToString, type SSRContext } from 'vue/server-renderer'
import privacy from '../../App/legal/privacy.md?raw'
import terms from '../../App/legal/terms.md?raw'
import HomePage from './pages/HomePage.vue'
import LegalPage from './pages/LegalPage.vue'

export { faq } from './faq'

export type Page = 'index' | 'terms' | 'privacy'

// 构建时（scripts/prerender.mjs）在 Node 里把三页渲染成 HTML，浏览器里（home.ts、terms.ts、privacy.ts）再接管：
// 搜索引擎、分享卡片不用跑脚本就读得到页面上的字。teleports：挪到 #teleports 里的（赞助的弹出框）
export async function render(page: Page): Promise<{ html: string; teleports: Record<string, string> }> {
  const app = page === 'index'
    ? createSSRApp(HomePage)
    : createSSRApp(LegalPage, { source: page === 'terms' ? terms : privacy })
  const context: SSRContext = {}
  const html = await renderToString(app, context)
  return { html, teleports: context.teleports ?? {} }
}
