<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref, watch } from 'vue'
import LogoMark from '@shared/LogoMark.vue'
import { ApiError, connect } from '../api'
import LineIcon from '../components/LineIcon.vue'
import { loadSdk, relayUrl, type Loaded, type Session } from '../remote'
import { displayId, findDevice, go, remember, state, takeHandoff, type System } from '../state'

// 会话（自己一个标签页，openSession 打开的）：上面一条细的工具栏，下面铺满对方的画面（照着程序的会话窗口）。
// 向服务器要通行证（我的设备还有一把一次性钥匙），SDK 的浏览器包经转发进程连过去（docs/web.md）：画面画在画布上
// （VideoView，在 SDK 的媒体线程上），键鼠经 InputMapper，对方的指针变成页面的指针（CursorView），剪贴板
// （ClipboardSync）、文件两个方向。网页版最高 1080p、每秒 30 帧
const props = defineProps<{ id: string; handoff: string }>()

const device = computed(() => findDevice(props.id))
const name = computed(() => device.value?.name ?? displayId(props.id))
// 对方的系统：连上以后按 SDK 说的，之前按记着的
const peerSystem = ref<System | null>(null)
const system = computed<System>(() => peerSystem.value ?? device.value?.system ?? 'other')

// 打开它的标签页交过来的临时密码；没有时（我的设备，或者刷新过、直接打开的地址）按需要在这里输
const password = ref(takeHandoff(props.handoff))
const typed = ref('')

// waiting：刚打开，还不知道要不要密码 | password：等输入临时密码 | connecting：正在连（等对方点允许也算）| connected |
// ended：没连上、断开了（ended 说为什么）
const phase = ref<'waiting' | 'password' | 'connecting' | 'connected' | 'ended'>('waiting')
const ended = ref('')
// 对方的电脑上弹出了连接请求，在等它点允许
const admitting = ref(false)
const clipboard = ref(true)
const fullScreen = ref(false)
const menu = ref<'' | 'shortcuts' | 'displays'>('')
const hintOpen = ref(true)
const canvas = ref<HTMLCanvasElement | null>(null)
const stage = ref<HTMLElement | null>(null)

let session: Session | null = null
let sdk: Loaded | null = null
let input: InstanceType<Loaded['rcsdk']['InputMapper']> | null = null
let sync: InstanceType<Loaded['rcsdk']['ClipboardSync']> | null = null
let closeViews = () => {}

// SDK 的结果码（rcs_result）给人看的说法
const NETWORK = -15
const AUTHENTICATION = -16
function failureText(result: number, message: string, wasConnected: boolean): string {
  switch (result) {
  case NETWORK: return wasConnected ? '连接断了：网络不稳定，或者对方的比邻关了' : '对方没有回应，过一会儿再试'
  case AUTHENTICATION: return '临时密码不对'
  case -22: return '对方没有允许这次连接'
  case -18: return '对方断开了连接'
  case -20: return '对方正忙，过一会儿再试'
  case -3:
  case -17: return '双方的比邻版本不一样，请对方更新到最新版本'
  default: return message ? `没连上（${message}）` : '没连上'
  }
}

function base64Bytes(text: string): Uint8Array {
  return Uint8Array.from(atob(text), (c) => c.charCodeAt(0))
}

function platformSystem(platform: number): System {
  return platform === 2 ? 'windows' : platform === 1 ? 'apple' : 'other'
}

function end(message: string) {
  phase.value = 'ended'
  ended.value = message
  admitting.value = false
  menu.value = ''
  displays.value = []
  stopStats()
  closeViews()
  closeViews = () => {}
  input = null
  sync = null
  void session?.destroy()
  session = null
}

async function start() {
  phase.value = 'connecting'
  ended.value = ''
  let answer: { ticket: string; key?: string }
  let loaded: Loaded
  try {
    [answer, loaded] = await Promise.all([connect(props.id), loadSdk()])
  } catch (error) {
    end(error instanceof ApiError ? error.message : `没连上（${error instanceof Error ? error.message : String(error)}）`)
    return
  }
  sdk = loaded
  // 服务器没给钥匙（不是我的设备，或者账号退出了）又没有临时密码：先输
  if (!answer.key && !password.value) {
    phase.value = 'password'
    return
  }
  const { rcsdk } = loaded
  const created = loaded.sdk.createSession({
    identity: answer.key ? { key: base64Bytes(answer.key) } : { pin: password.value },
    // 转发进程一条消息转一个数据报
    channel: 'datagram',
    // 和转发进程之间的连接一断就算断了（再连要新的通行证）
    reconnect: false,
    // 服务器的带宽是共用的：一路最多 6 Mbit/s（转发进程超过 8 Mbit/s 的丢掉）
    viewRequest: { width: 1920, height: 1080, framesPerSecond: 30, bitrateBps: 6_000_000, cursor: 'separate' },
    clipboard: { send: true, receive: true },
    files: { send: true, receive: true },
  })
  session = created
  const view = new rcsdk.VideoView(created, canvas.value!)
  input = new rcsdk.InputMapper(created, view, { paste: clipboard.value ? 'clipboard' : 'key' })
  const cursor = new rcsdk.CursorView(created, view)
  sync = clipboard.value ? new rcsdk.ClipboardSync(created) : null
  closeViews = () => {
    sync?.close()
    cursor.close()
    input?.close()
    view.close()
  }
  let wasConnected = false
  created.addEventListener('state', (event: Event) => {
    const { state: now, result, message } = (event as CustomEvent<{ state: string; result: number; message?: string }>).detail
    if (now === 'running' && !wasConnected) {
      wasConnected = true
      connected()
    } else if (now === 'failed') {
      // 密码不对：重新连接时再输一次
      if (result === AUTHENTICATION) password.value = ''
      end(failureText(result, message ?? '', wasConnected))
    } else if (now === 'stopped' && phase.value !== 'ended') {
      end('对方断开了连接')
    }
  })
  // 1：等对方点允许
  created.addEventListener('admission', (event: Event) => {
    admitting.value = (event as CustomEvent<{ state: number }>).detail.state === 1
  })
  // 转发进程那边满了（4503）
  created.addEventListener('socketclose', (event: Event) => {
    if ((event as CustomEvent<{ code: number }>).detail.code === 4503)
      end('网页版现在用的人太多了，过一会儿再试，或者下载客户端')
  })
  created.addEventListener('message', (event: Event) =>
    receivedControl((event as CustomEvent<{ channel: number; bytes: Uint8Array }>).detail))
  listenFiles(created)
  try {
    await created.connectWebSocket(relayUrl(answer.ticket))
    await created.start()
  } catch (error) {
    end(`没连上（${error instanceof Error ? error.message : String(error)}）`)
  }
}

function connected() {
  phase.value = 'connected'
  admitting.value = false
  canvas.value?.focus()
  const peer = platformSystem(session?.info().peer.platform ?? 0)
  peerSystem.value = peer
  remember(props.id, { name: device.value?.mine ? device.value.name : undefined, system: peer })
  startStats()
}

function submitPassword() {
  if (!typed.value.trim()) return
  password.value = typed.value.trim()
  typed.value = ''
  void start()
}

function retry() {
  if (password.value || device.value?.mine) void start()
  else phase.value = 'password'
}

// 刚打开：等问到账号（是不是我的设备），我的设备、带着密码的直接连，别的先输密码
watch(() => state.loaded, (loaded) => {
  if (!loaded || phase.value !== 'waiting') return
  if (password.value || device.value?.mine) void start()
  else phase.value = 'password'
}, { immediate: true })

// ---- 连着时的状态：多大、几帧、往返多久 ----

const status = ref('')
let statsTimer = 0
let lastFrames = 0

function startStats() {
  lastFrames = 0
  statsTimer = window.setInterval(() => {
    if (!session) return
    const stats = session.stats()
    const stream = session.info().stream
    const frames = stats.videoReceive.decodedFrames
    const fps = lastFrames > 0 ? frames - lastFrames : 0
    lastFrames = frames
    const roundTrip = Math.round(stats.network.roundTripNs / 1e6)
    status.value = [stream.width > 0 ? `${stream.width}×${stream.height}` : '', fps > 0 ? `${fps} 帧` : '',
                    roundTrip > 0 ? `${roundTrip} ms` : ''].filter(Boolean).join(' · ')
  }, 1000)
}

function stopStats() {
  window.clearInterval(statsTimer)
  status.value = ''
}

// ---- 浏览器、系统自己会拿走的组合键：从菜单里发给对方（HID 键码） ----

const Ctrl = 0xe0, Shift = 0xe1, Alt = 0xe2, Win = 0xe3
const shortcuts = computed(() => system.value === 'apple'
  ? [
      { title: '调度中心', keys: '⌃ ↑', codes: [Ctrl, 0x52] },
      { title: '显示桌面', keys: 'F11', codes: [0x44] },
      { title: '聚焦搜索', keys: '⌘ 空格', codes: [Win, 0x2c] },
      { title: '切换程序', keys: '⌘ Tab', codes: [Win, 0x2b] },
      { title: '强制退出', keys: '⌘ ⌥ Esc', codes: [Win, Alt, 0x29] },
      { title: '锁定屏幕', keys: '⌃ ⌘ Q', codes: [Ctrl, Win, 0x14] },
    ]
  : [
      { title: '安全选项', keys: 'Ctrl Alt Del', codes: [] },
      { title: '开始菜单', keys: 'Win', codes: [Win] },
      { title: '切换窗口', keys: 'Alt Tab', codes: [Alt, 0x2b] },
      { title: '任务管理器', keys: 'Ctrl Shift Esc', codes: [Ctrl, Shift, 0x29] },
      { title: '显示桌面', keys: 'Win D', codes: [Win, 0x07] },
      { title: '锁定', keys: 'Win L', codes: [Win, 0x0f] },
    ])

function sendShortcut(codes: number[]) {
  menu.value = ''
  if (!session) return
  // Ctrl+Alt+Del 系统不让程序按出来：SDK 另有一种（安全选项）
  if (codes.length === 0) {
    session.sendInput({ type: 'secureAttention' })
  } else {
    for (const code of codes) session.sendInput({ type: 'key', code, down: true })
    for (const code of [...codes].reverse()) session.sendInput({ type: 'key', code, down: false })
  }
  canvas.value?.focus()
}

// ---- 对方的显示器（程序的控制通道 2，rc/control_protocol.h）：几块时可以换着看 ----

interface Display { id: number; name: string; width: number; height: number; main: boolean }
const displays = ref<Display[]>([])
const currentDisplay = ref(0)
const ControlChannel = 2

function receivedControl(message: { channel: number; bytes: Uint8Array }) {
  if (message.channel !== ControlChannel) return
  const parsed = JSON.parse(new TextDecoder().decode(message.bytes)) as { type?: string; displays?: Display[]; current?: number }
  if (parsed.type === 'displays') {
    displays.value = parsed.displays ?? []
    currentDisplay.value = parsed.current ?? 0
  }
}

function showDisplay(id: number) {
  menu.value = ''
  session?.sendData(ControlChannel, new TextEncoder().encode(JSON.stringify({ type: 'showDisplay', id })))
  canvas.value?.focus()
}

// ---- 剪贴板同步：关掉时对方复制的不写进本机，Ctrl+V 只按键 ----

function toggleClipboard() {
  clipboard.value = !clipboard.value
  if (!session || !sdk || !input) return
  input.paste = clipboard.value ? 'clipboard' : 'key'
  if (clipboard.value) {
    sync = new sdk.rcsdk.ClipboardSync(session)
  } else {
    sync?.close()
    sync = null
  }
}

// ---- 文件：发给对方（选的、拖到画面上的），对方发来的问收不收，收下来存到下载 ----

interface Transfer {
  id: number
  sending: boolean
  name: string
  progress: number
  // offer：对方发来、等这边决定 | going | done | failed
  state: 'offer' | 'going' | 'done' | 'failed'
}
const transfers = ref<Transfer[]>([])
const fileInput = ref<HTMLInputElement | null>(null)
const dragging = ref(false)

function transferOf(id: number): Transfer | undefined {
  return transfers.value.find((t) => t.id === id)
}

function finish(id: number, ok: boolean) {
  const transfer = transferOf(id)
  if (!transfer) return
  transfer.state = ok ? 'done' : 'failed'
  if (ok) {
    transfer.progress = 1
    window.setTimeout(() => (transfers.value = transfers.value.filter((t) => t !== transfer)), 3000)
  }
}

function size(bytes: number): string {
  return bytes < 1024 * 1024 ? `${Math.max(1, Math.round(bytes / 1024))} KB` : `${(bytes / 1024 / 1024).toFixed(1)} MB`
}

function listenFiles(created: Session) {
  created.addEventListener('fileoffer', (event: Event) => {
    const { transfer, items, totalBytes } = (event as CustomEvent<{ transfer: number; items: number; totalBytes: number }>).detail
    const first = (created.files.get(transfer, { count: 1 }).items[0] ?? {}) as { name?: string; path?: string }
    const label = first.name ?? first.path ?? '文件'
    transfers.value.push({ id: transfer, sending: false, name: `${items > 1 ? `${label} 等 ${items} 项` : label}（${size(totalBytes)}）`,
                           progress: 0, state: 'offer' })
  })
  created.addEventListener('fileprogress', (event: Event) => {
    const { transfer, doneBytes, totalBytes } = (event as CustomEvent<{ transfer: number; doneBytes: number; totalBytes: number }>).detail
    const found = transferOf(transfer)
    if (found && totalBytes > 0) found.progress = doneBytes / totalBytes
  })
  created.addEventListener('filereceived', (event: Event) => {
    const { path, file } = (event as CustomEvent<{ path: string; file: File }>).detail
    const url = URL.createObjectURL(file)
    const link = document.createElement('a')
    link.href = url
    link.download = path.split('/').pop() ?? file.name
    link.click()
    window.setTimeout(() => URL.revokeObjectURL(url), 60_000)
  })
  created.addEventListener('filedone', (event: Event) => {
    const { transfer, result, itemsFailed } = (event as CustomEvent<{ transfer: number; result: number; itemsFailed: number }>).detail
    finish(transfer, result === 0 && itemsFailed === 0)
  })
}

function send(entries: Parameters<Session['files']['send']>[0], label: string) {
  if (!session || entries.length === 0) return
  const id = session.files.send(entries)
  transfers.value.push({ id, sending: true, name: label, progress: 0, state: 'going' })
}

async function sendPicked() {
  const files = fileInput.value?.files
  if (!files || files.length === 0 || !sdk) return
  send(await sdk.rcsdk.collectFiles(files), files.length > 1 ? `${files[0]!.name} 等 ${files.length} 个` : files[0]!.name)
  fileInput.value!.value = ''
}

async function dropped(event: DragEvent) {
  dragging.value = false
  if (!event.dataTransfer || !sdk || phase.value !== 'connected') return
  const first = event.dataTransfer.files[0]
  const count = event.dataTransfer.files.length
  const entries = await sdk.rcsdk.collectFiles(event.dataTransfer)
  send(entries, count > 1 ? `${first?.name} 等 ${count} 项` : first?.name ?? '文件')
}

function accept(transfer: Transfer) {
  session?.files.accept(transfer.id, { into: 'memory' })
  transfer.state = 'going'
}

function reject(transfer: Transfer) {
  session?.files.reject(transfer.id)
  transfers.value = transfers.value.filter((t) => t !== transfer)
}

function dismiss(transfer: Transfer) {
  transfers.value = transfers.value.filter((t) => t !== transfer)
}

// ---- 全屏：只剩画面；Chrome 在全屏时能把 Esc 这些键也交给页面（Keyboard Lock），按住 Esc 退出 ----

type KeyboardLock = { lock?: () => Promise<void>; unlock?: () => void }
const keyboard = (navigator as Navigator & { keyboard?: KeyboardLock }).keyboard

const onFullScreen = () => {
  fullScreen.value = document.fullscreenElement !== null
  if (fullScreen.value) void keyboard?.lock?.()
  else keyboard?.unlock?.()
}
const toggleFullScreen = () => {
  if (document.fullscreenElement) void document.exitFullscreen()
  else void stage.value?.requestFullscreen()
}

const closeMenu = (event: MouseEvent) => {
  if (!(event.target as HTMLElement).closest('.menu-holder')) menu.value = ''
}

// 连着时关标签页：先问一下
const beforeUnload = (event: BeforeUnloadEvent) => {
  if (phase.value === 'connected') event.preventDefault()
}

onMounted(() => {
  document.title = `${name.value} - 比邻网页版`
  document.addEventListener('fullscreenchange', onFullScreen)
  window.addEventListener('mousedown', closeMenu)
  window.addEventListener('beforeunload', beforeUnload)
})
onUnmounted(() => {
  document.title = '比邻网页版'
  stopStats()
  closeViews()
  void session?.destroy()
  document.removeEventListener('fullscreenchange', onFullScreen)
  window.removeEventListener('mousedown', closeMenu)
  window.removeEventListener('beforeunload', beforeUnload)
})

// 断开：先让 SDK 和对方好好告别（不然对方当成路断了），再关掉这个标签页（它是打开的那一页开出来的）；
// 关不掉时（直接打开的地址）回到首页
const disconnect = async () => {
  stopStats()
  closeViews()
  closeViews = () => {}
  const closing = session
  session = null
  phase.value = 'ended'
  await closing?.destroy()
  window.close()
  go('')
}
</script>

<template>
  <div class="session" :class="{ full: fullScreen }">
    <header class="bar">
      <LogoMark :size="20" class="logo" />
      <span class="os small" :class="system">
        <LineIcon :name="system === 'other' ? 'monitor' : system" :size="13" />
      </span>
      <span class="name">{{ name }}</span>
      <span v-if="phase === 'connected'" class="status">
        <span class="dot online" />经服务器转发<template v-if="status"> · {{ status }}</template>
      </span>

      <div class="tools">
        <div v-if="displays.length > 1" class="menu-holder">
          <button class="tool" type="button" :class="{ on: menu === 'displays' }" title="换一块显示器看"
                  @click="menu = menu === 'displays' ? '' : 'displays'">
            <LineIcon name="monitor" :size="16" /><LineIcon name="chevron-down" :size="12" />
          </button>
          <div v-if="menu === 'displays'" class="menu">
            <div class="menu-label">对方的显示器</div>
            <button v-for="(d, index) in displays" :key="d.id" type="button" :class="{ current: d.id === currentDisplay }"
                    @click="showDisplay(d.id)">
              <span>{{ d.name || `显示器 ${index + 1}` }}{{ d.main ? '（主）' : '' }}</span>
              <kbd>{{ d.width }}×{{ d.height }}</kbd>
            </button>
          </div>
        </div>
        <div class="menu-holder">
          <button class="tool" type="button" :class="{ on: menu === 'shortcuts' }" :disabled="phase !== 'connected'"
                  title="发送快捷键" @click="menu = menu === 'shortcuts' ? '' : 'shortcuts'">
            <LineIcon name="keyboard" :size="16" /><LineIcon name="chevron-down" :size="12" />
          </button>
          <div v-if="menu === 'shortcuts'" class="menu">
            <div class="menu-label">发送给 {{ name }}</div>
            <button v-for="s in shortcuts" :key="s.title" type="button" @click="sendShortcut(s.codes)">
              <span>{{ s.title }}</span><kbd>{{ s.keys }}</kbd>
            </button>
          </div>
        </div>
        <button class="tool" type="button" :class="{ on: clipboard }" :disabled="phase !== 'connected'"
                :title="clipboard ? '剪贴板同步：开（点一下关掉）' : '剪贴板同步：关（点一下打开）'" @click="toggleClipboard">
          <LineIcon name="clipboard" :size="16" />
        </button>
        <button class="tool" type="button" :disabled="phase !== 'connected'" title="发送文件（也可以拖到画面上）"
                @click="fileInput?.click()">
          <LineIcon name="upload" :size="16" />
        </button>
        <input ref="fileInput" type="file" multiple hidden @change="sendPicked" />
        <button class="tool" type="button" :disabled="phase !== 'connected'" title="全屏（按住 Esc 退出）" @click="toggleFullScreen">
          <LineIcon name="maximize" :size="16" />
        </button>
        <span class="divider" />
        <button class="button ghost danger small" type="button" @click="disconnect">断开</button>
      </div>
    </header>

    <main ref="stage" class="stage" @dragover.prevent="dragging = phase === 'connected'" @dragleave="dragging = false"
          @drop.prevent="dropped">
      <!-- 对方的画面：交给 SDK 的媒体线程画（VideoView），一直在，连上了才看得见 -->
      <canvas v-show="phase === 'connected'" ref="canvas" class="picture" tabindex="0" />

      <div v-if="phase === 'waiting'" class="overlay">
        <span class="spinner" />
      </div>

      <form v-if="phase === 'password'" class="overlay" @submit.prevent="submitPassword">
        <div class="overlay-title">连接 {{ name }}</div>
        <div class="overlay-detail">输入对方电脑上比邻显示的临时密码（设备 ID {{ displayId(props.id) }}）</div>
        <div class="password-row">
          <input v-model="typed" class="field" placeholder="临时密码" autocomplete="off" spellcheck="false" autofocus />
          <button class="button dark" type="submit" :disabled="!typed.trim()">连接</button>
        </div>
      </form>

      <div v-if="phase === 'connecting'" class="overlay">
        <span class="spinner" />
        <div class="overlay-title">正在连接 {{ name }}…</div>
        <div class="overlay-detail">{{ admitting ? '等对方在电脑上点"允许"…' : '网页版经比邻的服务器转发' }}</div>
        <button class="button" type="button" @click="disconnect">取消</button>
      </div>

      <div v-if="phase === 'ended'" class="overlay">
        <div class="overlay-title">{{ ended }}</div>
        <div class="overlay-detail">{{ name }}（{{ displayId(props.id) }}）</div>
        <div class="password-row centered">
          <button class="button dark" type="button" @click="retry">重新连接</button>
          <button class="button" type="button" @click="disconnect">关闭</button>
        </div>
      </div>

      <div v-if="dragging" class="drop-hint">松开就发给 {{ name }}</div>

      <div v-if="phase === 'connected' && hintOpen && !fullScreen" class="hint">
        <span>网页版最高 1080p、每秒 30 帧。想要更清晰流畅，可以
          <a href="https://www.bilindesk.com/#download" target="_blank" rel="noopener">下载客户端</a>。</span>
        <button class="icon-button" type="button" aria-label="知道了" @click="hintOpen = false">
          <LineIcon name="x" :size="14" />
        </button>
      </div>

      <div v-if="transfers.length > 0" class="transfers">
        <div v-for="t in transfers" :key="`${t.sending}-${t.id}`" class="transfer">
          <LineIcon :name="t.sending ? 'upload' : 'download'" :size="15" />
          <div class="transfer-text">
            <div class="transfer-name">
              {{ t.state === 'offer' ? '对方发来' : t.state === 'done' ? (t.sending ? '发送好了' : '收好了')
                : t.state === 'failed' ? (t.sending ? '没发出去' : '没收下来') : t.sending ? '正在发送' : '正在接收' }}
              {{ t.name }}
            </div>
            <div v-if="t.state === 'going' || t.state === 'done'" class="track"><div :style="{ width: `${t.progress * 100}%` }" /></div>
          </div>
          <template v-if="t.state === 'offer'">
            <button class="button small dark" type="button" @click="accept(t)">接收</button>
            <button class="button small" type="button" @click="reject(t)">拒绝</button>
          </template>
          <button v-else-if="t.state === 'failed'" class="icon-button" type="button" aria-label="知道了" @click="dismiss(t)">
            <LineIcon name="x" :size="14" />
          </button>
        </div>
      </div>
    </main>
  </div>
</template>

<style scoped>
.session {
  display: flex;
  flex-direction: column;
  height: 100%;
  background: var(--canvas);
}

.bar {
  display: flex;
  align-items: center;
  gap: 8px;
  flex-shrink: 0;
  height: 40px;
  padding: 0 8px 0 6px;
  background: var(--bg);
  box-shadow: 0 1px 0 var(--border);
}

.logo {
  flex-shrink: 0;
  margin: 0 4px 0 6px;
}

.os.small {
  width: 22px;
  height: 22px;
  border-radius: 6px;
}

.name {
  overflow: hidden;
  font-size: 13px;
  font-weight: 600;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.status {
  display: flex;
  align-items: center;
  gap: 6px;
  margin-left: 4px;
  overflow: hidden;
  font-size: 12px;
  color: var(--text-secondary);
  white-space: nowrap;
}

.tools {
  display: flex;
  align-items: center;
  gap: 2px;
  margin-left: auto;
}

.tool {
  display: inline-flex;
  align-items: center;
  gap: 2px;
  height: 28px;
  padding: 0 7px;
  border: 0;
  border-radius: var(--radius);
  background: transparent;
  color: var(--text-secondary);
  cursor: pointer;
}

.tool:hover:not(:disabled) {
  background: var(--hover);
  color: var(--text);
}

.tool.on {
  background: var(--selected);
  color: var(--text);
}

.tool:disabled {
  opacity: 0.35;
  cursor: default;
}

.divider {
  width: 1px;
  height: 16px;
  margin: 0 6px;
  background: var(--border);
}

.button.small {
  height: 28px;
  padding: 0 10px;
}

.menu-holder {
  position: relative;
}

.menu {
  position: absolute;
  top: calc(100% + 6px);
  right: 0;
  z-index: 10;
  width: 230px;
  padding: 5px;
  border-radius: 10px;
  background: var(--surface);
  box-shadow: var(--shadow);
}

.menu-label {
  padding: 6px 9px 4px;
  font-size: 12px;
  color: var(--text-secondary);
}

.menu button {
  display: flex;
  align-items: center;
  justify-content: space-between;
  width: 100%;
  height: 32px;
  padding: 0 9px;
  border: 0;
  border-radius: 6px;
  background: transparent;
  font-size: 13px;
  cursor: pointer;
}

.menu button:hover {
  background: var(--hover);
}

.menu button.current {
  font-weight: 600;
}

kbd {
  font-family: inherit;
  font-size: 12px;
  color: var(--text-secondary);
}

.stage {
  position: relative;
  display: grid;
  flex: 1;
  place-items: center;
  min-height: 0;
  overflow: hidden;
  background: var(--canvas);
}

/* 对方的画面：铺满，按比例放进去（SDK 把页面坐标换成画面坐标时也按 object-fit: contain 算） */
.picture {
  position: absolute;
  inset: 0;
  width: 100%;
  height: 100%;
  object-fit: contain;
  outline: none;
}

.overlay {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 8px;
  text-align: center;
}

.overlay-title {
  margin-top: 8px;
  font-size: 15px;
  font-weight: 600;
}

.overlay-detail {
  margin-bottom: 10px;
  font-size: 13px;
  color: var(--text-secondary);
}

.password-row {
  display: flex;
  gap: 8px;
  width: 300px;
  max-width: 100%;
}

.password-row .field {
  background: var(--surface);
}

.password-row.centered {
  justify-content: center;
}

.spinner {
  width: 26px;
  height: 26px;
  border: 2.5px solid var(--border);
  border-top-color: var(--text);
  border-radius: 50%;
  animation: spin 0.8s linear infinite;
}

@keyframes spin {
  to {
    transform: rotate(360deg);
  }
}

.hint,
.transfer {
  display: flex;
  align-items: center;
  gap: 10px;
  max-width: calc(100% - 32px);
  padding: 10px 10px 10px 14px;
  border-radius: 10px;
  background: var(--surface);
  box-shadow: var(--shadow);
  font-size: 13px;
}

.hint {
  position: absolute;
  right: 16px;
  bottom: 16px;
}

/* 互传的文件：右下角一张一张往上叠 */
.transfers {
  position: absolute;
  right: 16px;
  bottom: 72px;
  display: flex;
  flex-direction: column;
  gap: 8px;
  max-width: calc(100% - 32px);
}

.drop-hint {
  position: absolute;
  inset: 16px;
  display: grid;
  place-items: center;
  border: 2px dashed var(--text-secondary);
  border-radius: 14px;
  background: var(--scrim);
  font-size: 15px;
  font-weight: 600;
  pointer-events: none;
}

.hint a {
  font-weight: 600;
  text-decoration: underline;
  text-underline-offset: 2px;
}

.transfer {
  width: 340px;
  max-width: 100%;
  padding-right: 10px;
}

.transfer .button.small {
  height: 26px;
  padding: 0 10px;
}

.transfer-text {
  flex: 1;
  min-width: 0;
}

.transfer-name {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.track {
  height: 4px;
  margin-top: 6px;
  overflow: hidden;
  border-radius: 999px;
  background: var(--fill);
}

.track div {
  height: 100%;
  border-radius: 999px;
  background: var(--text);
  transition: width 0.15s;
}

@media (max-width: 640px) {
  .status {
    display: none;
  }
}
</style>
