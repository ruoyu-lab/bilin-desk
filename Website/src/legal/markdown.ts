// 服务协议、隐私政策是 App/legal 下的 Markdown（程序里也显示它们），只用到这几样：
// # 标题、## 小标题、段落、- 列表（续行缩进两格）、**加粗**。中文折行不加空格
export interface Inline {
  text: string
  bold: boolean
}

export type Block =
  | { kind: 'title'; text: string }
  | { kind: 'heading'; text: string }
  | { kind: 'paragraph'; content: Inline[] }
  | { kind: 'list'; items: Inline[][] }

export function parse(source: string): Block[] {
  const blocks: Block[] = []
  let paragraph: string | null = null
  let items: string[] | null = null

  const flush = () => {
    if (paragraph !== null)
      blocks.push({ kind: 'paragraph', content: inline(paragraph) })
    if (items !== null)
      blocks.push({ kind: 'list', items: items.map(inline) })
    paragraph = null
    items = null
  }

  for (const line of source.split('\n')) {
    if (line.trim() === '') {
      flush()
    } else if (line.startsWith('# ')) {
      flush()
      blocks.push({ kind: 'title', text: line.slice(2).trim() })
    } else if (line.startsWith('## ')) {
      flush()
      blocks.push({ kind: 'heading', text: line.slice(3).trim() })
    } else if (line.startsWith('- ')) {
      if (items === null)
        flush()
      items = [...(items ?? []), line.slice(2).trim()]
    } else if (items !== null && line.startsWith('  ')) {
      items[items.length - 1] += line.trim()
    } else {
      if (items !== null)
        flush()
      paragraph = (paragraph ?? '') + line.trim()
    }
  }
  flush()
  return blocks
}

// **加粗** 和普通文字交替
function inline(text: string): Inline[] {
  return text
    .split('**')
    .map((part, index) => ({ text: part, bold: index % 2 === 1 }))
    .filter((part) => part.text.length > 0)
}
