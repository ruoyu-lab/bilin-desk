import { computed, reactive, ref, watch } from 'vue'
import * as api from './api'

// 网页版的状态（docs/web.md）：登录着的账号和它下面的设备从服务器来（api.ts），连过的、收藏的记在这个浏览器里

export type System = 'windows' | 'apple' | 'other'

export interface Device {
  id: string // 9 位数字
  name: string
  system: System
  // 在不在线：我的设备才知道（服务器说的）；别人的电脑不知道（null）
  online: boolean | null
  // 我的设备：同一个账号下的，不用密码
  mine: boolean
  favorite: boolean
  // 上次在这个浏览器里连接的时间（ISO）；没连过时为空
  lastConnected: string
}

// 记在浏览器里的一台：连过的、收藏的
interface Remembered {
  id: string
  name: string
  system: System
  favorite: boolean
  lastConnected: string
}

export const state = reactive({
  // 问过服务器了（刚打开时还没问到，先不显示"没登录"）
  loaded: false,
  account: null as api.Account | null,
  accountDevices: [] as api.AccountDevice[],
  records: [] as Remembered[],
})

// "416902934" → "416 902 934"
export function displayId(id: string): string {
  return id.replace(/(\d{3})(?=\d)/g, '$1 ')
}

export function systemOf(platform: string): System {
  return platform === 'Windows' ? 'windows' : platform === 'macOS' ? 'apple' : 'other'
}

// ---- 记在浏览器里的：几个标签页共用（storage 事件），存不了（隐私模式）时只在这个页面里 ----

const RECORDS = 'bilin.devices'

function readRecords(): Remembered[] {
  try {
    const stored = JSON.parse(localStorage.getItem(RECORDS) ?? '[]') as unknown
    return Array.isArray(stored) ? stored as Remembered[] : []
  } catch {
    return []
  }
}

function saveRecords(): void {
  try {
    localStorage.setItem(RECORDS, JSON.stringify(state.records))
  } catch {
    // 存不了：这次照样用
  }
}

state.records = readRecords()
window.addEventListener('storage', (event) => {
  if (event.key === RECORDS) state.records = readRecords()
})

function record(id: string): Remembered {
  let found = state.records.find((r) => r.id === id)
  if (!found) {
    found = { id, name: displayId(id), system: 'other', favorite: false, lastConnected: '' }
    state.records.push(found)
  }
  return found
}

// 连上了：记进最近；知道了名字、系统的也记下
export function remember(id: string, known: { name?: string; system?: System }): void {
  const found = record(id)
  found.lastConnected = new Date().toISOString()
  if (known.name) found.name = known.name
  if (known.system && known.system !== 'other') found.system = known.system
  saveRecords()
}

export function toggleFavorite(device: Device): void {
  const found = record(device.id)
  found.name = device.name
  found.system = device.system
  found.favorite = !device.favorite
  saveRecords()
}

// 从最近里去掉（收藏的留着）
export function forget(device: Device): void {
  const found = record(device.id)
  if (found.favorite) found.lastConnected = ''
  else state.records = state.records.filter((r) => r.id !== device.id)
  saveRecords()
}

// ---- 账号：问服务器 ----

function applyMe(me: api.Me): void {
  state.account = me.account
  state.accountDevices = me.devices
  state.loaded = true
}

export async function refresh(): Promise<void> {
  try {
    applyMe(await api.me())
  } catch {
    // 连不上服务器：留着上次的，下次再问
    state.loaded = true
  }
}

export async function signIn(email: string, password: string): Promise<void> {
  applyMe(await api.login(email, password))
}

export async function registerAccount(nickname: string, email: string, password: string, code: string): Promise<void> {
  applyMe(await api.register(nickname, email, password, code))
}

export async function resetPassword(email: string, code: string, password: string): Promise<void> {
  applyMe(await api.resetPassword(email, code, password))
}

export async function signOut(): Promise<void> {
  await api.logout().catch(() => null)
  applyMe({ account: null, devices: [] })
}

// 打开时问一次；页面看得见的时候每 30 秒问一次（在线的变了），回到页面时马上问
void refresh()
window.setInterval(() => {
  if (document.visibilityState === 'visible') void refresh()
}, 30_000)
document.addEventListener('visibilitychange', () => {
  if (document.visibilityState === 'visible') void refresh()
})

// ---- 合起来的设备：我的设备（服务器的，能连的电脑）加上记在浏览器里的 ----

export const devices = computed<Device[]>(() => {
  const all = new Map<string, Device>()
  for (const d of state.account ? state.accountDevices : []) {
    const system = systemOf(d.platform)
    // 手机、平板不能被连
    if (system === 'other') continue
    const found = state.records.find((r) => r.id === d.deviceId)
    all.set(d.deviceId, {
      id: d.deviceId, name: d.name, system, online: d.online, mine: true,
      favorite: found?.favorite ?? false, lastConnected: found?.lastConnected ?? '',
    })
  }
  for (const r of state.records) {
    if (!all.has(r.id)) all.set(r.id, { ...r, online: null, mine: false })
  }
  return [...all.values()]
})

export function findDevice(id: string): Device | undefined {
  return devices.value.find((d) => d.id === id)
}

// 在线的在前
export function myDevices(): Device[] {
  return devices.value.filter((d) => d.mine).sort((a, b) => Number(b.online) - Number(a.online))
}

export function recentDevices(): Device[] {
  return devices.value.filter((d) => d.lastConnected).sort((a, b) => b.lastConnected.localeCompare(a.lastConnected))
}

export function favoriteDevices(): Device[] {
  return devices.value.filter((d) => d.favorite).sort((a, b) => Number(b.online === true) - Number(a.online === true))
}

// ---- 外观：跟随系统 | 浅色 | 深色，记在这个浏览器里 ----

export type Scheme = 'system' | 'light' | 'dark'

function storedScheme(): Scheme {
  try {
    const value = localStorage.getItem('bilin.scheme')
    return value === 'light' || value === 'dark' ? value : 'system'
  } catch {
    return 'system'
  }
}

export const scheme = ref<Scheme>(storedScheme())
export function setScheme(value: Scheme): void {
  scheme.value = value
}
watch(scheme, (value) => {
  if (value === 'system') document.documentElement.removeAttribute('data-theme')
  else document.documentElement.dataset.theme = value
  try {
    localStorage.setItem('bilin.scheme', value)
  } catch {
    // 存不了（隐私模式）：这次照样换
  }
}, { immediate: true })

// ---- 地址的 #：#/（概览）、#/devices、#/favorites、#/recent、#/settings，
// 会话 #/session/<设备 ID>/<交接编号>（新的标签页，见 openSession）----

export type Page = 'home' | 'devices' | 'favorites' | 'recent' | 'settings' | 'session' | 'unsupported'

function parse(): { page: Page; id: string; handoff: string } {
  const [, page = '', id = '', handoff = ''] = location.hash.split('/')
  if (page === 'session' && /^\d{9}$/.test(id)) return { page: 'session', id, handoff }
  if (page === 'devices' || page === 'favorites' || page === 'recent' || page === 'settings')
    return { page, id: '', handoff: '' }
  // 预览"请用 Chrome 或 Edge 打开"那一页（只在本机开发时）
  if (page === 'unsupported' && import.meta.env.DEV) return { page, id: '', handoff: '' }
  return { page: 'home', id: '', handoff: '' }
}

export const route = ref(parse())
window.addEventListener('hashchange', () => (route.value = parse()))

export function go(path: string): void {
  location.hash = `#/${path}`
}

// SDK 的浏览器包只在 Chromium 内核的浏览器（Chrome、Edge）里验证过：别的先不让用
export const supported = (navigator as Navigator & { userAgentData?: { brands: Array<{ brand: string }> } })
  .userAgentData?.brands.some((b) => b.brand === 'Chromium') ?? false

// 离现在多久：刚才、5 分钟前、3 小时前、昨天、3 天前
export function ago(iso: string): string {
  const minutes = (Date.now() - new Date(iso).getTime()) / 60_000
  if (minutes < 1) return '刚才'
  if (minutes < 60) return `${Math.floor(minutes)} 分钟前`
  if (minutes < 24 * 60) return `${Math.floor(minutes / 60)} 小时前`
  if (minutes < 48 * 60) return '昨天'
  return `${Math.floor(minutes / 60 / 24)} 天前`
}

// ---- 连接、登录：哪一页点的都一样，对话框在 App 里 ----

// 等输入临时密码的那一台
export const askingPassword = ref<Device | null>(null)
export const loginOpen = ref(false)

// 模板里不能给导入的 ref 赋值：经这几个函数改
export function openLogin(): void {
  loginOpen.value = true
}

export function closeLogin(): void {
  loginOpen.value = false
}

export function closePassword(): void {
  askingPassword.value = null
}

// 我的设备不用密码；别的要临时密码
export function connect(device: Device): void {
  if (device.mine) openSession(device.id)
  else askingPassword.value = device
}

// 按设备 ID 连（概览上输入的）：我的设备不要密码
export function connectById(id: string, password: string): void {
  openSession(id, findDevice(id)?.mine ? '' : password)
}

// 会话在新的标签页里，几台可以一起连。临时密码不放进地址：以一个随机的交接编号存进这个浏览器，
// 新的标签页拿到就删掉（takeHandoff），地址里只有编号
const HANDOFF = 'bilin.handoff.'

export function openSession(id: string, password = ''): void {
  const handoff = crypto.randomUUID()
  if (password) localStorage.setItem(HANDOFF + handoff, password)
  const url = `${location.pathname}#/session/${id}/${handoff}`
  // 不用 noopener：带着它 window.open 总是返回 null，分不出是不是被拦了。打开以后自己断开 opener
  const tab = window.open(url, '_blank')
  if (tab) tab.opener = null
  // 浏览器拦了新窗口：就在这个标签页里打开
  else location.href = url
}

// 交过来的临时密码（只能拿一次）；没有时为空：我的设备，或者刷新过、直接打开的地址，要再输一次
export function takeHandoff(handoff: string): string {
  const key = HANDOFF + handoff
  const password = localStorage.getItem(key) ?? ''
  localStorage.removeItem(key)
  return password
}
