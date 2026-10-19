import type * as Rcsdk from '../../third_party/rcsdk/web/rcsdk'

// SDK 的浏览器包（public/sdk/，scripts/build-web-sdk.sh 编的）经转发进程（/relay）连电脑（docs/web.md）；
// 通行证向服务器要（api.ts 的 connect）

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

// 转发进程的地址：和网页同一个主机。通行证只能用一次、一分钟就过期，放在地址里没关系
export function relayUrl(ticket: string): string {
  const scheme = location.protocol === 'https:' ? 'wss' : 'ws'
  return `${scheme}://${location.host}/relay?ticket=${encodeURIComponent(ticket)}`
}
