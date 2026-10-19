// 网页版的接口（服务端的 Server/src/web.ts，docs/web.md）：都在同一个主机的 /web/ 下。
// 登录以后是 HttpOnly 的 cookie，页面的脚本碰不到，fetch 同源时自己带上

export interface Account {
  email: string
  nickname: string
}

// 账号下的一台设备
export interface AccountDevice {
  deviceId: string
  name: string
  platform: string
  online: boolean
}

export interface Me {
  account: Account | null
  devices: AccountDevice[]
}

export class ApiError extends Error {
  constructor(readonly code: string, message: string, readonly reason = '') {
    super(message)
  }
}

// 服务端说的原因给人看的说法
const messages: Record<string, string> = {
  invalid: '填得不对，检查一下',
  wrongPassword: '邮箱或密码不对',
  wrongCode: '验证码不对或者过期了，重新发一个试试',
  emailTaken: '这个邮箱已经注册过了',
  noAccount: '这个邮箱还没有注册过',
  tooMany: '试得太频繁了，过一会儿再试',
  mailLimit: '今天发出的验证码太多了，明天再试',
  mailFailed: '验证码邮件没发出去，过一会儿再试',
  origin: '请从比邻网页版的页面操作',
  offline: '对方的电脑不在线',
  busy: '网页版现在用的人太多了，过一会儿再试，或者下载客户端',
  unavailable: '网页版暂时用不了，过一会儿再试',
  noAnswer: '对方没有回应：对方的比邻可能不是最新版本，请对方更新以后再试',
}

// 电脑说不行的原因（rc/peer_protocol.h）
const declined: Record<string, string> = {
  notAccepting: '对方没有打开"允许别人连接本机"',
  transportBusy: '对方的电脑正被别人直连着，暂时没法用网页版连',
  needsPermission: '对方的电脑还没授予比邻录屏的权限',
  unavailable: '对方的比邻出了点问题，过一会儿再试',
}

async function call<T>(method: 'GET' | 'POST', path: string, body?: object): Promise<T> {
  const response = await fetch(`/web/${path}`, {
    method,
    credentials: 'same-origin',
    headers: body ? { 'content-type': 'application/json' } : {},
    body: body ? JSON.stringify(body) : undefined,
  }).catch(() => null)
  if (!response) throw new ApiError('network', '连不上比邻的服务器，检查一下网络')
  const data = await response.json().catch(() => null) as (T & { error?: string; reason?: string }) | null
  if (response.ok && data) return data
  const code = data?.error ?? String(response.status)
  if (code === 'declined')
    throw new ApiError(code, declined[data?.reason ?? ''] ?? '对方现在不接受连接', data?.reason ?? '')
  throw new ApiError(code, messages[code] ?? '服务器没答应，过一会儿再试')
}

export const me = () => call<Me>('GET', 'me')
export const login = (email: string, password: string) => call<Me>('POST', 'login', { email, password })
export const logout = () => call<{ ok: true }>('POST', 'logout', {})
export const sendCode = (email: string, purpose: 'register' | 'resetPassword') =>
  call<{ ok: true }>('POST', 'code', { email, purpose })
export const register = (nickname: string, email: string, password: string, code: string) =>
  call<Me>('POST', 'register', { nickname, email, password, code })
export const resetPassword = (email: string, code: string, password: string) =>
  call<Me>('POST', 'reset', { email, code, password })

// 要连 deviceId：电脑准备好了才回答（进了转发进程里的房间）。key（base64）：浏览器登录了那台电脑的账号，
// 这次连接用它，不要临时密码
export const connect = (deviceId: string) => call<{ ticket: string; key?: string }>('POST', 'connect', { deviceId })
