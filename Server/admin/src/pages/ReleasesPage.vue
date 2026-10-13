<script setup lang="ts">
import { onMounted, ref } from 'vue'
import { api, ApiError, base, platformName, platforms, signedIn, type PlatformReleases, type Release, type SignedRelease } from '../api'
import LineIcon from '../components/LineIcon.vue'
import { bytes, time } from '../format'
import { toast } from '../toasts'

// 版本发布：选好安装包和它的签名文件（scripts/sign-release.mjs 写的 .release.json），上传，服务器核对签名
const list = ref<PlatformReleases[]>([])
const error = ref('')
const over = ref(false)
// 选好了、还没上传的
const pending = ref<{ file: File; release: SignedRelease } | null>(null)
// 上传了多少（0~1）；null 是还没开始
const progress = ref<number | null>(null)

const errors: Record<string, string> = {
  signature: '签名不对：安装包和签名文件对不上，或者不是用发布密钥签的',
  notNewer: '这个版本不比现在发布的新',
  version: '版本号不对',
  platform: '不认识的平台',
  empty: '安装包是空的',
}

async function load() {
  try {
    list.value = (await api<{ platforms: PlatformReleases[] }>('GET', 'releases')).platforms
  } catch {
    // 没登录时 api 已经回到登录页
  }
}
onMounted(load)

async function remove(platform: string, release: Release) {
  if (!confirm(`删除 ${platformName(platform)} ${release.version}？`)) return
  try {
    await api('DELETE', `releases/${platform}/${release.version}`)
    toast(`删除了 ${platformName(platform)} ${release.version}`)
  } catch (e) {
    if (!(e instanceof ApiError && e.code === 'signedOut')) toast('删除没成', true)
  }
  await load()
}

// 要一起选安装包和它的 .release.json（同一个名字）
async function choose(files: File[]) {
  error.value = ''
  pending.value = null
  progress.value = null
  const installer = files.find((f) => f.name.endsWith('.pkg') || f.name.endsWith('.exe'))
  const signature = files.find((f) => f.name.endsWith('.release.json'))
  if (files.length !== 2 || !installer || !signature || signature.name !== `${installer.name}.release.json`) {
    error.value = '要一起选安装包和它的 .release.json（同一个名字）'
    return
  }
  let release: SignedRelease
  try {
    release = JSON.parse(await signature.text()) as SignedRelease
  } catch {
    error.value = '签名文件读不出来'
    return
  }
  if (release.size !== installer.size) {
    error.value = '签名文件说的大小和安装包不一样：安装包签名以后又重新打过？'
    return
  }
  pending.value = { file: installer, release }
}

function picked(event: Event) {
  const input = event.target as HTMLInputElement
  void choose([...(input.files ?? [])])
  input.value = ''
}

function dropped(event: DragEvent) {
  over.value = false
  void choose([...(event.dataTransfer?.files ?? [])])
}

// 上传要一会儿：用 XMLHttpRequest 看进度
function publish() {
  const chosen = pending.value
  if (!chosen) return
  const { file, release } = chosen
  error.value = ''
  progress.value = 0
  const request = new XMLHttpRequest()
  request.open('PUT', `${base}api/releases/${release.platform}/${release.version}`)
  request.setRequestHeader('content-type', 'application/octet-stream')
  request.setRequestHeader('x-release-signature', release.signature)
  request.upload.onprogress = (event) => (progress.value = event.loaded / event.total)
  request.onloadend = async () => {
    if (request.status === 401) {
      signedIn.value = false
      return
    }
    if (request.status !== 200) {
      let code: string | null = null
      try {
        code = (JSON.parse(request.responseText) as { error?: string }).error ?? null
      } catch {
        // 不是接口的回答（比如 Nginx 的错误页）
      }
      progress.value = null
      error.value = (code && errors[code]) || '发布没成，看服务器上 bilin-admin 的日志（pm2 logs bilin-admin）'
      return
    }
    pending.value = null
    progress.value = null
    toast(`发布了 ${platformName(release.platform)} ${release.version}`)
    await load()
  }
  request.send(file)
}
</script>

<template>
  <section class="page">
    <div class="page-head">
      <div>
        <h1>版本发布</h1>
        <p>发布以后，各台电脑上的程序会在左侧导航栏提示有新版本，点了就更新；官网的下载按钮也换成它。只收发布密钥签了名、比现在新的版本。</p>
      </div>
    </div>

    <div class="card">
      <label
        class="drop"
        :class="{ over }"
        @dragenter.prevent="over = true"
        @dragover.prevent="over = true"
        @dragleave="over = false"
        @drop.prevent="dropped"
      >
        <div class="tile"><LineIcon name="upload" :size="22" /></div>
        <strong>把安装包和它的签名文件拖到这里，或者点这里选择</strong>
        <span>.pkg 或 .exe，和 scripts/sign-release.mjs 写的同名 .release.json</span>
        <input type="file" multiple accept=".pkg,.exe,.json" hidden @change="picked" />
      </label>
      <div v-if="pending" class="pending">
        <div class="tile"><LineIcon :name="platforms[pending.release.platform]?.icon ?? 'package'" :size="22" /></div>
        <div class="main">
          <div class="title">{{ platformName(pending.release.platform) }} {{ pending.release.version }}</div>
          <div v-if="progress === null" class="meta">{{ pending.file.name }} · {{ bytes(pending.file.size) }}</div>
          <template v-else>
            <div class="meta num">
              上传中 {{ Math.floor(progress * 100) }}% · {{ bytes(progress * pending.file.size) }} / {{ bytes(pending.file.size) }}
            </div>
            <div class="track"><div :style="{ width: `${progress * 100}%` }"></div></div>
          </template>
        </div>
        <button class="button small ghost" type="button" :disabled="progress !== null" @click="pending = null">取消</button>
        <button class="button small primary" type="button" :disabled="progress !== null" @click="publish">发布</button>
      </div>
      <p v-if="error" class="error message">{{ error }}</p>
    </div>

    <div class="grid two">
      <div v-for="p in list" :key="p.platform" class="card">
        <div class="platform">
          <div class="tile"><LineIcon :name="platforms[p.platform]?.icon ?? 'package'" :size="22" /></div>
          <div>
            <div class="name">{{ platformName(p.platform) }}</div>
            <div class="version num">{{ p.current ?? '还没有发布' }}</div>
          </div>
        </div>
        <div v-if="p.releases.length > 0" class="rows">
          <div v-for="r in p.releases" :key="r.version" class="row">
            <div class="main">
              <div class="title num">
                {{ r.version }}<span v-if="r.version === p.current" class="tag">现在的版本</span>
              </div>
              <div class="meta num">{{ bytes(r.size) }} · {{ time(r.publishedAt) }}</div>
            </div>
            <div class="actions">
              <button
                v-if="r.version !== p.current"
                class="button small ghost danger"
                type="button"
                @click="remove(p.platform, r)"
              >
                删除
              </button>
            </div>
          </div>
        </div>
      </div>
    </div>
  </section>
</template>

<style scoped>
.drop {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 8px;
  padding: 30px 20px;
  border: 2px dashed #d5d8de;
  border-radius: 14px;
  background: var(--bg);
  text-align: center;
  cursor: pointer;
  transition: border-color 0.15s, background 0.15s;
}

.drop:hover,
.drop.over {
  border-color: var(--text);
}

.drop .tile {
  margin-bottom: 4px;
  background: var(--fill);
}

.drop strong {
  font-size: 15px;
  font-weight: 600;
}

.drop span {
  font-size: 12px;
  color: var(--text-muted);
}

.pending {
  display: flex;
  align-items: center;
  gap: 12px;
  margin-top: 14px;
  padding: 14px 16px;
  border-radius: 14px;
  background: var(--bg);
}

.pending .main {
  flex: 1;
  min-width: 0;
}

.pending .title {
  font-size: 15px;
  font-weight: 600;
}

.pending .meta {
  margin-top: 2px;
  font-size: 12px;
  color: var(--text-muted);
}

.pending .track {
  margin-top: 10px;
}

.message {
  margin-top: 14px;
}

.platform {
  display: flex;
  align-items: center;
  gap: 12px;
  margin-bottom: 18px;
}

.platform .name {
  font-size: 13px;
  color: var(--text-secondary);
}

.platform .version {
  margin-top: 1px;
  font-size: 22px;
  font-weight: 800;
}
</style>
