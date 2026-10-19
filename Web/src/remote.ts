import type * as Rcsdk from '../../third_party/rcsdk/web/rcsdk'

// 真的连接（docs/web.md）：服务器发通行证（POST /web/connect），SDK 的浏览器包（public/sdk/，
// scripts/build-web-sdk.sh 编的）经转发进程（/relay）连过去

export type Sdk = Rcsdk.Sdk
export type Session = Rcsdk.Session

export interface Loaded {
  rcsdk: typeof Rcsdk // 包里的类：VideoView、InputMapper……
  sdk: Sdk            // 加载好的模块：createSession、这个浏览器能解什么
}

let loading: Promise<Loaded> | null = null

// SDK 的浏览器包：用到时才加载（几 MB），一个页面只加载一次
export function loadSdk(): Promise<Loaded> {
  loading ??= (async () => {
    const rcsdk = await import(/* @vite-ignore */ `${import.meta.env.BASE_URL}sdk/rcsdk.mjs`) as typeof Rcsdk
    return { rcsdk, sdk: await rcsdk.load() }
  })()
  return loading
}

export class ConnectError extends Error {}

// 要连 deviceId：服务器告诉那台电脑有浏览器要来，回给这个浏览器进它房间的一次性通行证
export async function requestTicket(deviceId: string): Promise<string> {
  const response = await fetch('/web/connect', {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ deviceId }),
  }).catch(() => null)
  if (!response) throw new ConnectError('连不上比邻的服务器，检查一下网络')
  const data = await response.json().catch(() => null) as { ticket?: string; error?: string } | null
  if (response.ok && data?.ticket) return data.ticket
  if (data?.error === 'offline') throw new ConnectError('对方的电脑不在线')
  if (data?.error === 'tooMany') throw new ConnectError('连得太频繁了，过一会儿再试')
  throw new ConnectError('服务器没答应，过一会儿再试')
}

// 转发进程的地址：和网页同一个主机。通行证只能用一次、一分钟就过期，放在地址里没关系
export function relayUrl(ticket: string): string {
  const scheme = location.protocol === 'https:' ? 'wss' : 'ws'
  return `${scheme}://${location.host}/relay?ticket=${encodeURIComponent(ticket)}`
}
