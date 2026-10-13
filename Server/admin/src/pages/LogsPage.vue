<script setup lang="ts">
import { onMounted, ref } from 'vue'
import { api, ApiError, type LogEntry } from '../api'
import LineIcon from '../components/LineIcon.vue'
import { time } from '../format'
import { toast } from '../toasts'

// 出错日志：服务端、管理后台最近的警告和出错（Server/src/admin/logs.ts），新的在前
const entries = ref<LogEntry[] | null>(null)
const loading = ref(false)

async function load() {
  loading.value = true
  try {
    entries.value = (await api<{ entries: LogEntry[] }>('GET', 'logs')).entries
  } catch {
    // 没登录时 api 已经回到登录页
  } finally {
    loading.value = false
  }
}
onMounted(load)

// 清空：服务器上服务端、管理后台的日志文件真的清掉、删掉（平时的运行记录也在里面），找不回来
async function clear() {
  if (!confirm('清空服务端、管理后台的全部日志？\n\n日志文件会被清掉、删掉，平时的运行记录也在里面，清了就找不回来。')) return
  try {
    await api('DELETE', 'logs')
    toast('日志清空了')
  } catch (e) {
    if (!(e instanceof ApiError && e.code === 'signedOut')) toast('没清成，看看服务器上的日志目录', true)
  }
  await load()
}

const appNames: Record<string, string> = { 'bilin-server': '服务端', 'bilin-admin': '管理后台' }
</script>

<template>
  <section class="page">
    <div class="page-head">
      <div>
        <h1>出错日志</h1>
        <p>服务端、管理后台最近的警告和出错，最多 100 条。日志在服务器上只留 14 天，也可以清空。</p>
      </div>
      <div class="actions">
        <button class="button ghost danger" type="button" :disabled="loading" @click="clear">清空</button>
        <button class="button" type="button" :disabled="loading" @click="load">
          <LineIcon name="refresh" :size="16" />{{ loading ? '正在读取…' : '刷新' }}
        </button>
      </div>
    </div>

    <div class="card">
      <p v-if="entries === null" class="empty">正在读取…</p>
      <p v-else-if="entries.length === 0" class="empty">最近没有警告和出错</p>
      <div v-else class="rows">
        <div v-for="(e, i) in entries" :key="i" class="row entry">
          <span class="level" :class="e.level">{{ e.level === 'error' ? '出错' : '警告' }}</span>
          <div class="main">
            <div class="message">{{ e.message }}</div>
            <div v-if="e.detail" class="detail">{{ e.detail }}</div>
          </div>
          <div class="where">
            <div class="num">{{ e.time ? time(e.time) : '—' }}</div>
            <div>{{ appNames[e.app] ?? e.app }}</div>
          </div>
        </div>
      </div>
    </div>
  </section>
</template>

<style scoped>
.actions {
  display: flex;
  gap: 8px;
}

.entry {
  align-items: flex-start;
}

.level {
  flex-shrink: 0;
  padding: 2px 8px;
  border-radius: 999px;
  font-size: 12px;
  font-weight: 600;
}

.level.error {
  background: #fbe4e2;
  color: var(--danger);
}

.level.warn {
  background: #f8edd9;
  color: #9a5b00;
}

.message {
  font-size: 14px;
  overflow-wrap: anywhere;
}

.detail {
  margin-top: 3px;
  font-family: ui-monospace, "SF Mono", Menlo, Consolas, monospace;
  font-size: 12px;
  color: var(--text-muted);
  overflow-wrap: anywhere;
}

.where {
  flex-shrink: 0;
  text-align: right;
  font-size: 12px;
  color: var(--text-muted);
}
</style>
