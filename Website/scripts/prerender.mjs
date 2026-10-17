// 构建的最后一步（package.json 的 build）：用 vite build --ssr 编出来的 dist-ssr/server.js 把三页渲染成 HTML，
// 填进 dist 里的页面（浏览器里再接管），首页再写上结构化数据（JSON-LD）；最后写 sitemap.xml，删掉 dist-ssr。
// 搜索引擎、分享卡片不用跑脚本就读得到页面上的字
import { readFile, rm, writeFile } from 'node:fs/promises'

const site = 'https://www.bilindesk.com/'
const dist = new URL('../dist/', import.meta.url)
const ssr = new URL('../dist-ssr/', import.meta.url)
const { render, faq } = await import(new URL('server.js', ssr))

// 搜索结果里的软件信息、常见问题（页面上显示的是同一份，src/faq.ts）
const structured = [
  {
    '@context': 'https://schema.org',
    '@type': 'SoftwareApplication',
    name: '比邻',
    alternateName: 'Bilin',
    url: site,
    image: `${site}og.png`,
    description: '免费、开源的远程桌面软件，支持 Windows 和 macOS，也能用 Android 手机连接、控制电脑，全程端到端加密。',
    applicationCategory: 'UtilitiesApplication',
    operatingSystem: 'Windows 10, Windows 11, macOS 14, Android 10',
    downloadUrl: `${site}#download`,
    license: 'https://www.gnu.org/licenses/agpl-3.0.html',
    isAccessibleForFree: true,
    offers: { '@type': 'Offer', price: '0', priceCurrency: 'CNY' },
  },
  {
    '@context': 'https://schema.org',
    '@type': 'FAQPage',
    mainEntity: faq.map(({ question, answer }) => ({
      '@type': 'Question',
      name: question,
      acceptedAnswer: { '@type': 'Answer', text: answer },
    })),
  },
]

const pages = [
  { name: 'index', path: '' },
  { name: 'terms', path: 'terms.html' },
  { name: 'privacy', path: 'privacy.html' },
]

for (const page of pages) {
  const file = new URL(`${page.name}.html`, dist)
  let html = await readFile(file, 'utf8')
  const { html: app, teleports } = await render(page.name)
  if (!html.includes('<div id="app"></div>'))
    throw new Error(`${page.name}.html 里没有 <div id="app"></div>`)
  html = html.replace('<div id="app"></div>', () => `<div id="app">${app}</div>`)
    .replace('<div id="teleports"></div>', () => `<div id="teleports">${teleports['#teleports'] ?? ''}</div>`)
  if (page.name === 'index') {
    // </script> 不能出现在里面：JSON 里的 < 写成 <
    const json = JSON.stringify(structured).replace(/</g, '\\u003c')
    html = html.replace('</head>', () => `  <script type="application/ld+json">${json}</script>\n  </head>`)
  }
  await writeFile(file, html)
}

const today = new Date().toISOString().slice(0, 10)
const urls = pages.map(({ path }) => `  <url><loc>${site}${path}</loc><lastmod>${today}</lastmod></url>`).join('\n')
await writeFile(new URL('sitemap.xml', dist),
  `<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n${urls}\n</urlset>\n`)
await rm(ssr, { recursive: true })
console.log('预渲染了', pages.map(page => `${page.name}.html`).join('、'), '，写了 sitemap.xml')
