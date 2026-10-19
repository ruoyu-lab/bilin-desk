import { reactive, ref, watch } from 'vue'

// 网页版的状态。现在是假数据，先把界面做出来（docs/web.md）；接上服务器和 SDK 的浏览器包以后换成真的

export type System = 'windows' | 'apple'

export interface Device {
  id: string // 9 位数字
  name: string
  system: System
  online: boolean
  // 我的设备：同一个账号下的，不用密码
  mine: boolean
  favorite: boolean
  // 上次连接的时间（ISO）；没连过时为空
  lastConnected: string
}

export interface Account {
  email: string
  nickname: string
}

const hoursAgo = (hours: number) => new Date(Date.now() - hours * 3600_000).toISOString()

export const state = reactive({
  account: null as Account | null,
  devices: [
    { id: '416902934', name: '书房的 PC', system: 'windows', online: true, mine: true, favorite: true, lastConnected: hoursAgo(2) },
    { id: '731204586', name: '客厅的 Mac mini', system: 'apple', online: true, mine: true, favorite: false, lastConnected: hoursAgo(30) },
    { id: '208553617', name: '爸妈家的笔记本', system: 'windows', online: false, mine: true, favorite: false, lastConnected: hoursAgo(120) },
    { id: '582017463', name: '小林的 MacBook', system: 'apple', online: true, mine: false, favorite: false, lastConnected: hoursAgo(5) },
    { id: '903361258', name: '阿杰的游戏本', system: 'windows', online: true, mine: false, favorite: false, lastConnected: hoursAgo(72) },
  ] as Device[],
})

export function signIn(email: string): void {
  state.account = { email, nickname: '阿禾' }
}

export function signOut(): void {
  state.account = null
}

// "416902934" → "416 902 934"
export function displayId(id: string): string {
  return id.replace(/(\d{3})(?=\d)/g, '$1 ')
}

export function findDevice(id: string): Device | undefined {
  return state.devices.find((d) => d.id === id)
}

// 我的设备只有登录着才有
export function myDevices(): Device[] {
  return state.account ? state.devices.filter((d) => d.mine) : []
}

export function recentDevices(): Device[] {
  return state.devices.filter((d) => d.lastConnected && (!d.mine || state.account))
    .sort((a, b) => b.lastConnected.localeCompare(a.lastConnected))
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
  if (device.mine && state.account) openSession(device.id)
  else askingPassword.value = device
}

// 按设备 ID 连（概览上输入的）：没见过的先记进最近
export function connectById(id: string, password: string): void {
  if (!findDevice(id))
    state.devices.push({ id, name: displayId(id), system: 'windows', online: true, mine: false, favorite: false, lastConnected: '' })
  openSession(id, password)
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

export function favoriteDevices(): Device[] {
  return state.devices.filter((d) => d.favorite && (!d.mine || state.account))
}

export function toggleFavorite(device: Device): void {
  device.favorite = !device.favorite
}
