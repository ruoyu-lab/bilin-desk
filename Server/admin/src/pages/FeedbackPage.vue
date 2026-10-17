<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import { api, ApiError, base, type Feedback } from '../api'
import LineIcon from '../components/LineIcon.vue'
import { ago, bytes, time } from '../format'
import { toast } from '../toasts'

// 用户反馈：程序里提交的（Server/src/feedback.ts），新的在前，90 天后服务器自己删掉。
// 左边是列表，右边是选中的那一条；选中就记成看过了。日志在新标签页里看
const items = ref<Feedback[] | null>(null)
const loading = ref(false)
const selectedId = ref<string | null>(null)
const onlyUnread = ref(false)

const unread = computed(() => items.value?.filter((f) => !f.read).length ?? 0)
const shown = computed(() => (items.value ?? []).filter((f) => !onlyUnread.value || !f.read || f.id === selectedId.value))
const selected = computed(() => items.value?.find((f) => f.id === selectedId.value) ?? null)

// 有新的反馈时发邮件到哪里：空的是不发
const notify = ref<string | null>(null)
const notifyTo = ref('')
const savingNotify = ref(false)

async function load() {
  loading.value = true
  try {
    const data = await api<{ feedback: Feedback[]; notify: string | null }>('GET', 'feedback')
    items.value = data.feedback
    notify.value = data.notify
    notifyTo.value = data.notify ?? ''
    if (!data.feedback.some((f) => f.id === selectedId.value)) selectedId.value = null
  } catch {
    // 没登录时 api 已经回到登录页
  } finally {
    loading.value = false
  }
}
onMounted(load)

async function select(item: Feedback) {
  selectedId.value = item.id
  if (item.read) return
  try {
    Object.assign(item, await api<Feedback>('POST', `feedback/${item.id}/read`))
  } catch {
    // 删掉了、到期了：刷新一下列表
    await load()
  }
}

async function remove(item: Feedback) {
  if (!confirm('删除这条反馈？连同它带的日志，删了就找不回来。')) return
  try {
    await api('DELETE', `feedback/${item.id}`)
    toast('删除了')
  } catch (e) {
    if (!(e instanceof ApiError && e.code === 'signedOut')) toast('没删成', true)
  }
  await load()
}

async function saveNotify() {
  savingNotify.value = true
  try {
    notify.value = (await api<{ notify: string | null }>('PUT', 'feedback/notify', { to: notifyTo.value })).notify
    notifyTo.value = notify.value ?? ''
    toast(notify.value ? '有新的反馈时发邮件到这个邮箱' : '有新的反馈时不发邮件了')
  } catch (e) {
    if (e instanceof ApiError && e.code === 'email') toast('邮箱不对', true)
    else if (!(e instanceof ApiError && e.code === 'signedOut')) toast('没保存成', true)
  } finally {
    savingNotify.value = false
  }
}

const logsUrl = (item: Feedback) => `${base}api/feedback/${item.id}/logs`
// 列表里只看第一段，换行压成空格
const preview = (text: string) => text.replace(/\s+/g, ' ')
</script>

<template>
  <section class="page">
    <div class="page-head">
      <div>
        <h1>用户反馈</h1>
        <p>用户在程序里提交的，带着版本、系统和日志，90 天后自动删除。</p>
      </div>
      <button class="button" type="button" :disabled="loading" @click="load">
        <LineIcon name="refresh" :size="16" />{{ loading ? '正在读取…' : '刷新' }}
      </button>
    </div>

    <form class="card notify" @submit.prevent="saveNotify">
      <span class="tile"><LineIcon name="mail" :size="18" /></span>
      <div class="notify-text">
        <strong>通知邮箱</strong>
        <span>{{ notify ? '有新的反馈时发邮件过去，一天最多 20 封' : '现在不发邮件，填上邮箱就会在有新的反馈时通知你' }}</span>
      </div>
      <input v-model="notifyTo" class="field" type="email" placeholder="邮箱，空着就不发" />
      <button class="button primary" type="submit" :disabled="savingNotify || notifyTo.trim() === (notify ?? '')">
        {{ savingNotify ? '正在保存…' : '保存' }}
      </button>
    </form>

    <div class="inbox">
      <div class="card list">
        <div class="list-head">
          <div class="filters">
            <button type="button" :class="{ current: !onlyUnread }" @click="onlyUnread = false">
              全部 <span class="num">{{ items?.length ?? 0 }}</span>
            </button>
            <button type="button" :class="{ current: onlyUnread }" @click="onlyUnread = true">
              没看 <span class="num">{{ unread }}</span>
            </button>
          </div>
        </div>
        <p v-if="items === null" class="empty">正在读取…</p>
        <p v-else-if="shown.length === 0" class="empty">{{ onlyUnread ? '都看过了' : '还没有反馈' }}</p>
        <div v-else class="items">
          <button
            v-for="f in shown"
            :key="f.id"
            type="button"
            class="item"
            :class="{ selected: f.id === selectedId, unread: !f.read }"
            @click="select(f)"
          >
            <div class="item-top">
              <span class="dot"></span>
              <span class="who">{{ f.deviceName }}</span>
              <span class="when">{{ ago(f.createdAt) }}</span>
            </div>
            <div class="item-text">{{ preview(f.text) }}</div>
            <div class="item-meta">
              <span>{{ f.platform }} {{ f.version }}</span>
              <span v-if="f.logsBytes > 0">· 日志 {{ bytes(f.logsBytes) }}</span>
              <span v-if="f.contact">· 留了联系方式</span>
            </div>
          </button>
        </div>
      </div>

      <div class="card detail">
        <div v-if="!selected" class="placeholder">
          <LineIcon name="message" :size="28" :stroke="1.5" />
          <span>{{ items && items.length > 0 ? '选一条反馈看详情' : '有人提交反馈后会出现在这里' }}</span>
        </div>
        <template v-else>
          <div class="detail-head">
            <div>
              <h2>{{ selected.deviceName }}</h2>
              <div class="sub">{{ time(selected.createdAt) }} · {{ ago(selected.createdAt) }}</div>
            </div>
            <div class="detail-actions">
              <a v-if="selected.logsBytes > 0" class="button small" :href="logsUrl(selected)" target="_blank" rel="noopener">
                <LineIcon name="download" :size="15" />看日志
              </a>
              <button class="button small ghost danger" type="button" @click="remove(selected)">
                <LineIcon name="trash" :size="15" />删除
              </button>
            </div>
          </div>

          <p class="text">{{ selected.text }}</p>

          <dl class="facts">
            <dt>联系方式</dt>
            <dd>
              <template v-if="selected.contact">
                {{ selected.contact }}<span v-if="selected.signedIn" class="tag light">账号</span>
              </template>
              <span v-else class="muted">没留</span>
            </dd>
            <dt>版本</dt>
            <dd>{{ selected.platform }} {{ selected.version }}</dd>
            <dt>系统</dt>
            <dd>{{ selected.system }}</dd>
            <dt>设备 ID</dt>
            <dd class="num">{{ selected.deviceId }}</dd>
            <dt>日志</dt>
            <dd>
              <template v-if="selected.logsBytes > 0">{{ bytes(selected.logsBytes) }}</template>
              <span v-else class="muted">没带</span>
            </dd>
          </dl>
        </template>
      </div>
    </div>
  </section>
</template>

<style scoped>
/* 通知邮箱：一条横的，图标、说明、输入框、保存 */
.notify {
  display: flex;
  align-items: center;
  gap: 14px;
  padding: 16px 18px;
}

.notify-text {
  display: flex;
  flex: 1;
  flex-direction: column;
  gap: 2px;
  min-width: 0;
}

.notify-text strong {
  font-size: 14px;
  font-weight: 600;
}

.notify-text span {
  font-size: 12px;
  color: var(--text-muted);
}

.notify .field {
  width: 280px;
  height: 38px;
  background: var(--bg);
}

/* 左边列表，右边详情；详情跟着滚动停在上面 */
.inbox {
  display: grid;
  grid-template-columns: minmax(280px, 380px) minmax(0, 1fr);
  gap: 16px;
  align-items: start;
}

.list {
  padding: 8px;
}

.list-head {
  padding: 6px 6px 10px;
}

.filters {
  display: inline-flex;
  gap: 2px;
  padding: 3px;
  border-radius: 10px;
  background: var(--fill);
}

.filters button {
  padding: 5px 12px;
  border: 0;
  border-radius: 8px;
  background: none;
  font-size: 13px;
  color: var(--text-secondary);
  cursor: pointer;
}

.filters button.current {
  background: var(--bg);
  color: var(--text);
  font-weight: 500;
}

.filters .num {
  margin-left: 2px;
  color: var(--text-muted);
}

.items {
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.item {
  display: flex;
  flex-direction: column;
  gap: 4px;
  width: 100%;
  padding: 12px 12px 12px 14px;
  border: 0;
  border-radius: 10px;
  background: none;
  text-align: left;
  cursor: pointer;
  transition: background 0.15s;
}

.item:hover {
  background: var(--fill);
}

.item.selected {
  background: var(--bg);
}

.item-top {
  display: flex;
  align-items: center;
  gap: 8px;
}

.dot {
  flex-shrink: 0;
  width: 7px;
  height: 7px;
  border-radius: 50%;
  background: transparent;
}

.unread .dot {
  background: var(--text);
}

.who {
  flex: 1;
  min-width: 0;
  overflow: hidden;
  font-size: 14px;
  color: var(--text-secondary);
  text-overflow: ellipsis;
  white-space: nowrap;
}

.unread .who {
  color: var(--text);
  font-weight: 600;
}

.when {
  flex-shrink: 0;
  font-size: 12px;
  color: var(--text-muted);
}

.item-text {
  display: -webkit-box;
  padding-left: 15px;
  overflow: hidden;
  font-size: 13px;
  line-height: 1.6;
  color: var(--text-secondary);
  overflow-wrap: anywhere;
  -webkit-box-orient: vertical;
  -webkit-line-clamp: 2;
}

.unread .item-text {
  color: var(--text);
}

.item-meta {
  display: flex;
  flex-wrap: wrap;
  gap: 4px;
  padding-left: 15px;
  font-size: 12px;
  color: var(--text-muted);
}

.detail {
  position: sticky;
  top: 24px;
  min-height: 360px;
  padding: 28px 30px;
}

.placeholder {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 12px;
  min-height: 304px;
  font-size: 13px;
  color: var(--text-muted);
}

.detail-head {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  gap: 16px;
}

.detail-head h2 {
  font-size: 20px;
  font-weight: 800;
}

.sub {
  margin-top: 4px;
  font-size: 13px;
  color: var(--text-muted);
}

.detail-actions {
  display: flex;
  flex-shrink: 0;
  gap: 6px;
}

.text {
  margin: 22px 0 0;
  padding: 18px 20px;
  border-radius: 12px;
  background: var(--bg);
  font-size: 15px;
  line-height: 1.8;
  white-space: pre-wrap;
  overflow-wrap: anywhere;
}

.facts {
  display: grid;
  grid-template-columns: 72px minmax(0, 1fr);
  gap: 10px 16px;
  margin: 22px 0 0;
  font-size: 14px;
}

.facts dt {
  color: var(--text-muted);
}

.facts dd {
  margin: 0;
  overflow-wrap: anywhere;
}

.facts .tag {
  margin-left: 8px;
}

.muted {
  color: var(--text-muted);
}

@media (max-width: 900px) {
  .inbox {
    grid-template-columns: minmax(0, 1fr);
  }
  .detail {
    position: static;
  }
  .notify {
    flex-wrap: wrap;
  }
  /* 说明占一行，输入框和保存在下一行 */
  .notify-text {
    flex-basis: calc(100% - 52px);
  }
  .notify .field {
    flex: 1;
    width: auto;
  }
}
</style>
