<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref } from 'vue'
import LogoMark from '@shared/LogoMark.vue'
import LineIcon from '../components/LineIcon.vue'
import { ConnectError, loadSdk, relayUrl, requestTicket, type Session } from '../remote'
import { displayId, findDevice, go, takeHandoff } from '../state'

// 会话（自己一个标签页，openSession 打开的）：上面一条细的工具栏，下面铺满对方的画面（照着程序的会话窗口）。
// 向服务器要通行证，SDK 的浏览器包经转发进程连过去（docs/web.md）：画面画在画布上（VideoView，在 SDK 的媒体线程上），
// 键鼠经 InputMapper，对方的指针变成页面的指针（CursorView）。网页版最高 1080p、每秒 30 帧
const props = defineProps<{ id: string; handoff: string }>()

const device = computed(() => findDevice(props.id))
const name = computed(() => device.value?.name ?? displayId(props.id))
const isMac = computed(() => device.value?.system === 'apple')

// 打开它的标签页交过来的临时密码；没有时（刷新过、直接打开的地址）先在这里输
const password = ref(takeHandoff(props.handoff))
const typed = ref('')

// password：等输入临时密码 | connecting：正在连（等对方点允许也算）| connected | ended：没连上、断开了（ended 说为什么）
const phase = ref<'password' | 'connecting' | 'connected' | 'ended'>(password.value ? 'connecting' : 'password')
const ended = ref('')
const clipboard = ref(true)
const fullScreen = ref(false)
const menuOpen = ref(false)
const hintOpen = ref(true)
const sending = ref<{ name: string; progress: number } | null>(null)
const canvas = ref<HTMLCanvasElement | null>(null)

// 对方的电脑上弹出了连接请求，在等它点允许
const waiting = ref(false)

let session: Session | null = null
let closeViews = () => {}

// SDK 的结果码（rcs_result）给人看的说法
const AUTHENTICATION = -16
function failureText(result: number, message: string): string {
  switch (result) {
  case -15: return '对方没有回应：对方的电脑可能正被别人直连着，或者比邻不是最新版本'
  case AUTHENTICATION: return '临时密码不对'
  case -22: return '对方没有允许这次连接'
  case -18: return '对方断开了连接'
  case -20: return '对方正忙，过一会儿再试'
  case -3:
  case -17: return '双方的比邻版本不一样，请对方更新到最新版本'
  default: return message ? `没连上（${message}）` : '没连上'
  }
}

function end(message: string) {
  phase.value = 'ended'
  waiting.value = false
  ended.value = message
  closeViews()
  closeViews = () => {}
  void session?.destroy()
  session = null
}

async function start() {
  phase.value = 'connecting'
  ended.value = ''
  try {
    const [ticket, { rcsdk, sdk }] = await Promise.all([requestTicket(props.id), loadSdk()])
    const created = sdk.createSession({
      identity: { pin: password.value },
      // 转发进程一条消息转一个数据报
      channel: 'datagram',
      viewRequest: { width: 1920, height: 1080, framesPerSecond: 30, cursor: 'separate' },
      clipboard: { send: true, receive: true },
      files: { send: true, receive: true },
    })
    session = created
    const view = new rcsdk.VideoView(created, canvas.value!)
    const input = new rcsdk.InputMapper(created, view)
    const cursor = new rcsdk.CursorView(created, view)
    const sync = new rcsdk.ClipboardSync(created)
    closeViews = () => {
      sync.close()
      cursor.close()
      input.close()
      view.close()
    }
    created.addEventListener('state', (event: Event) => {
      const { state, result, message } = (event as CustomEvent<{ state: string; result: number; message?: string }>).detail
      if (state === 'running') {
        phase.value = 'connected'
        waiting.value = false
      } else if (state === 'failed') {
        // 密码不对：重新连接时再输一次
        if (result === AUTHENTICATION) password.value = ''
        end(failureText(result, message ?? ''))
      } else if (state === 'stopped' && phase.value !== 'ended') {
        end('对方断开了连接')
      }
    })
    // 1：等对方点允许
    created.addEventListener('admission', (event: Event) => {
      waiting.value = (event as CustomEvent<{ state: number }>).detail.state === 1
    })
    created.addEventListener('fileprogress', (event: Event) => {
      const { doneBytes, totalBytes } = (event as CustomEvent<{ doneBytes: number; totalBytes: number }>).detail
      if (sending.value && totalBytes > 0) sending.value.progress = doneBytes / totalBytes
    })
    created.addEventListener('filedone', () => {
      if (sending.value) sending.value.progress = 1
      window.setTimeout(() => (sending.value = null), 1200)
    })
    await created.connectWebSocket(relayUrl(ticket))
    await created.start()
  } catch (error) {
    end(error instanceof ConnectError ? error.message : `没连上：${error instanceof Error ? error.message : String(error)}`)
  }
}

function submitPassword() {
  if (!typed.value.trim()) return
  password.value = typed.value.trim()
  typed.value = ''
  void start()
}

function retry() {
  if (password.value) void start()
  else phase.value = 'password'
}

onMounted(() => {
  document.title = `${name.value} - 比邻网页版`
  if (phase.value === 'connecting')
    void start()
  document.addEventListener('fullscreenchange', onFullScreen)
  window.addEventListener('mousedown', closeMenu)
})
onUnmounted(() => {
  document.title = '比邻网页版'
  closeViews()
  void session?.destroy()
  document.removeEventListener('fullscreenchange', onFullScreen)
  window.removeEventListener('mousedown', closeMenu)
})

// 浏览器、系统自己会拿走的组合键：从菜单里发给对方（还没接上）
const shortcuts = computed(() => isMac.value
  ? [
      { title: '调度中心', keys: '⌃ ↑' },
      { title: '显示桌面', keys: 'F11' },
      { title: '聚焦搜索', keys: '⌘ 空格' },
      { title: '切换程序', keys: '⌘ Tab' },
      { title: '强制退出', keys: '⌘ ⌥ Esc' },
      { title: '锁定屏幕', keys: '⌃ ⌘ Q' },
    ]
  : [
      { title: '安全选项', keys: 'Ctrl Alt Del' },
      { title: '开始菜单', keys: 'Win' },
      { title: '切换窗口', keys: 'Alt Tab' },
      { title: '任务管理器', keys: 'Ctrl Shift Esc' },
      { title: '显示桌面', keys: 'Win D' },
      { title: '锁定', keys: 'Win L' },
    ])

const stage = ref<HTMLElement | null>(null)
const onFullScreen = () => (fullScreen.value = document.fullscreenElement !== null)
const toggleFullScreen = () => {
  if (document.fullscreenElement) void document.exitFullscreen()
  else void stage.value?.requestFullscreen()
}

const closeMenu = (event: MouseEvent) => {
  if (!(event.target as HTMLElement).closest('.shortcuts')) menuOpen.value = false
}

const fileInput = ref<HTMLInputElement | null>(null)
const sendFiles = async () => {
  const files = fileInput.value?.files
  if (!files || files.length === 0 || !session) return
  const { rcsdk } = await loadSdk()
  sending.value = { name: files.length > 1 ? `${files[0]!.name} 等 ${files.length} 个` : files[0]!.name, progress: 0 }
  session.files.send(await rcsdk.collectFiles(files))
  fileInput.value!.value = ''
}

// 断开：关掉这个标签页（它是打开的那一页开出来的）；关不掉时（直接打开的地址）回到首页
const disconnect = () => {
  closeViews()
  void session?.destroy()
  session = null
  window.close()
  go('')
}
</script>

<template>
  <div class="session" :class="{ full: fullScreen }">
    <header class="bar">
      <LogoMark :size="20" class="logo" />
      <span class="os small" :class="device?.system ?? 'windows'">
        <LineIcon :name="device?.system ?? 'windows'" :size="13" />
      </span>
      <span class="name">{{ name }}</span>
      <span v-if="phase === 'connected'" class="status">
        <span class="dot online" />经服务器转发 · 最高 1080p、30 帧
      </span>

      <div class="tools">
        <div class="shortcuts">
          <button class="tool" type="button" :class="{ on: menuOpen }" :disabled="phase !== 'connected'"
                  title="发送快捷键" @click="menuOpen = !menuOpen">
            <LineIcon name="keyboard" :size="16" /><LineIcon name="chevron-down" :size="12" />
          </button>
          <div v-if="menuOpen" class="menu">
            <div class="menu-label">发送给 {{ name }}</div>
            <button v-for="s in shortcuts" :key="s.title" type="button" @click="menuOpen = false">
              <span>{{ s.title }}</span><kbd>{{ s.keys }}</kbd>
            </button>
          </div>
        </div>
        <button class="tool" type="button" :class="{ on: clipboard }" :disabled="phase !== 'connected'"
                :title="clipboard ? '剪贴板同步：开（点一下关掉）' : '剪贴板同步：关（点一下打开）'" @click="clipboard = !clipboard">
          <LineIcon name="clipboard" :size="16" />
        </button>
        <button class="tool" type="button" :disabled="phase !== 'connected'" title="发送文件" @click="fileInput?.click()">
          <LineIcon name="upload" :size="16" />
        </button>
        <input ref="fileInput" type="file" multiple hidden @change="sendFiles" />
        <button class="tool" type="button" :disabled="phase !== 'connected'" title="全屏" @click="toggleFullScreen">
          <LineIcon name="maximize" :size="16" />
        </button>
        <span class="divider" />
        <button class="button ghost danger small" type="button" @click="disconnect">断开</button>
      </div>
    </header>

    <main ref="stage" class="stage">
      <!-- 对方的画面：交给 SDK 的媒体线程画（VideoView），一直在，连上了才看得见 -->
      <canvas v-show="phase === 'connected'" ref="canvas" class="picture" tabindex="0" />

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
        <div class="overlay-detail">{{ waiting ? '等对方在电脑上点"允许"…' : '网页版经比邻的服务器转发' }}</div>
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

      <div v-if="phase === 'connected' && hintOpen && !fullScreen" class="hint">
        <span>网页版最高 1080p、每秒 30 帧。想要更清晰流畅，可以
          <a href="https://www.bilindesk.com/#download" target="_blank" rel="noopener">下载客户端</a>。</span>
        <button class="icon-button" type="button" aria-label="知道了" @click="hintOpen = false">
          <LineIcon name="x" :size="14" />
        </button>
      </div>

      <div v-if="sending" class="transfer">
        <LineIcon name="upload" :size="15" />
        <div class="transfer-text">
          <div class="transfer-name">{{ sending.progress >= 1 ? '发送好了' : '正在发送' }} {{ sending.name }}</div>
          <div class="track"><div :style="{ width: `${sending.progress * 100}%` }" /></div>
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

.shortcuts {
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
  position: absolute;
  right: 16px;
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
  bottom: 16px;
}

.hint a {
  font-weight: 600;
  text-decoration: underline;
  text-underline-offset: 2px;
}

.transfer {
  bottom: 72px;
  width: 300px;
  padding-right: 14px;
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
