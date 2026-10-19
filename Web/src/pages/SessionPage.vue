<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref } from 'vue'
import LineIcon from '../components/LineIcon.vue'
import { displayId, findDevice, go } from '../state'

// 会话：上面一条细的工具栏，下面铺满对方的画面（照着程序的会话窗口）。现在画面是假的，先把样子定下来；
// 接上 SDK 的浏览器包以后是 VideoView、InputMapper、CursorView（docs/web.md）
const props = defineProps<{ id: string }>()

const device = computed(() => findDevice(props.id))
const name = computed(() => device.value?.name ?? displayId(props.id))
const isMac = computed(() => device.value?.system === 'apple')

// connecting：正在连 | connected | ended：断开了
const phase = ref<'connecting' | 'connected' | 'ended'>('connecting')
const clipboard = ref(true)
const fullScreen = ref(false)
const menuOpen = ref(false)
const hintOpen = ref(true)
const sending = ref<{ name: string; progress: number } | null>(null)

let connectTimer = 0
onMounted(() => {
  document.title = `${name.value} - 比邻网页版`
  connectTimer = window.setTimeout(() => (phase.value = 'connected'), 1200)
  document.addEventListener('fullscreenchange', onFullScreen)
  window.addEventListener('mousedown', closeMenu)
})
onUnmounted(() => {
  document.title = '比邻网页版'
  window.clearTimeout(connectTimer)
  document.removeEventListener('fullscreenchange', onFullScreen)
  window.removeEventListener('mousedown', closeMenu)
})

// 浏览器、系统自己会拿走的组合键：从菜单里发给对方
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
const sendFiles = () => {
  const files = fileInput.value?.files
  if (!files || files.length === 0) return
  sending.value = { name: files.length > 1 ? `${files[0]!.name} 等 ${files.length} 个` : files[0]!.name, progress: 0 }
  const timer = window.setInterval(() => {
    if (!sending.value) return window.clearInterval(timer)
    sending.value.progress = Math.min(1, sending.value.progress + 0.08)
    if (sending.value.progress >= 1) {
      window.clearInterval(timer)
      window.setTimeout(() => (sending.value = null), 1200)
    }
  }, 150)
  fileInput.value!.value = ''
}

const disconnect = () => go('')
</script>

<template>
  <div class="session" :class="{ full: fullScreen }">
    <header class="bar">
      <a href="#/" class="back" aria-label="回到首页" title="回到首页">
        <LineIcon name="chevron-right" :size="16" style="transform: rotate(180deg)" />
      </a>
      <span class="os small" :class="device?.system ?? 'windows'">
        <LineIcon :name="device?.system ?? 'windows'" :size="13" />
      </span>
      <span class="name">{{ name }}</span>
      <span v-if="phase === 'connected'" class="status">
        <span class="dot online" />经服务器转发 · 1080p · 30 帧 · 32 ms
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
      <!-- 假的画面：对方的桌面 -->
      <div v-if="phase === 'connected'" class="picture" :class="{ mac: isMac }">
        <div class="window w1"><span class="window-bar" /></div>
        <div class="window w2"><span class="window-bar" /></div>
        <div class="taskbar" />
      </div>

      <div v-if="phase === 'connecting'" class="overlay">
        <span class="spinner" />
        <div class="overlay-title">正在连接 {{ name }}…</div>
        <div class="overlay-detail">网页版经比邻的服务器转发</div>
        <button class="button" type="button" @click="disconnect">取消</button>
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

.back {
  display: grid;
  place-items: center;
  width: 28px;
  height: 28px;
  border-radius: var(--radius);
  color: var(--text-secondary);
}

.back:hover {
  background: var(--hover);
  color: var(--text);
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

/* 假的画面：16:9，按窗口大小放进去 */
.picture {
  position: relative;
  width: min(100%, calc((100vh - 40px) * 16 / 9));
  aspect-ratio: 16 / 9;
  overflow: hidden;
  background: linear-gradient(135deg, #1f6fd1 0%, #5aa9f0 45%, #a8d4ff 100%);
}

.full .picture {
  width: min(100%, calc(100vh * 16 / 9));
}

.picture.mac {
  background: linear-gradient(160deg, #3b2f63 0%, #a2577c 50%, #f0a37a 100%);
}

.window {
  position: absolute;
  border-radius: 8px;
  background: #f7f8fb;
  box-shadow: 0 10px 30px rgba(0, 0, 0, 0.25);
}

.window-bar {
  display: block;
  height: 9%;
  min-height: 14px;
  border-radius: 8px 8px 0 0;
  background: rgba(0, 0, 0, 0.06);
}

.w1 {
  top: 12%;
  left: 10%;
  width: 46%;
  height: 58%;
}

.w2 {
  top: 26%;
  left: 44%;
  width: 40%;
  height: 50%;
}

.taskbar {
  position: absolute;
  right: 0;
  bottom: 0;
  left: 0;
  height: 6%;
  background: rgba(20, 24, 32, 0.75);
}

.mac .taskbar {
  top: 0;
  bottom: auto;
  height: 3.5%;
  background: rgba(255, 255, 255, 0.35);
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
