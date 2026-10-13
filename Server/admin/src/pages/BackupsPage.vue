<script setup lang="ts">
import { onMounted, ref } from 'vue'
import { api, ApiError, base, type Backup } from '../api'
import LineIcon from '../components/LineIcon.vue'
import { bytes, time } from '../format'
import { toast } from '../toasts'

// 数据备份：每天自动一份（服务器上只留最近 14 份），也能马上备份一份、下载到自己的电脑上
const backups = ref<Backup[]>([])
const running = ref(false)

async function load() {
  try {
    const result = await api<{ backups: Backup[]; running: boolean }>('GET', 'backups')
    backups.value = result.backups
    running.value = result.running
  } catch {
    // 没登录时 api 已经回到登录页
  }
}
onMounted(load)

async function backupNow() {
  running.value = true
  try {
    await api('POST', 'backups')
    toast('备份好了')
  } catch (e) {
    if (!(e instanceof ApiError && e.code === 'signedOut'))
      toast('备份没成，看服务器上 bilin-admin 的日志（pm2 logs bilin-admin）', true)
  }
  await load()
}

async function remove(backup: Backup) {
  if (!confirm(`删除 ${time(backup.createdAt)} 的备份？`)) return
  try {
    await api('DELETE', `backups/${encodeURIComponent(backup.name)}`)
    toast('删除了一份备份')
  } catch (e) {
    if (!(e instanceof ApiError && e.code === 'signedOut')) toast('删除没成', true)
  }
  await load()
}
</script>

<template>
  <section class="page">
    <div class="page-head">
      <div>
        <h1>数据备份</h1>
        <p>每天凌晨 4 点（服务器的时间）自动备份一次，服务器上只留最近 14 份。想另外留一份时，下载到自己的电脑上。</p>
      </div>
      <button class="button primary" type="button" :disabled="running" @click="backupNow">
        <LineIcon name="database" :size="18" />{{ running ? '正在备份…' : '立即备份' }}
      </button>
    </div>

    <div class="card">
      <div v-if="backups.length === 0" class="empty">还没有备份</div>
      <div v-else class="rows">
        <div v-for="(backup, i) in backups" :key="backup.name" class="row">
          <div class="tile"><LineIcon name="database" /></div>
          <div class="main">
            <div class="title num">
              {{ time(backup.createdAt) }}<span v-if="i === 0" class="tag light">最新</span>
            </div>
            <div class="meta num">{{ bytes(backup.size) }}</div>
          </div>
          <div class="actions">
            <a class="button small ghost" :href="`${base}api/backups/${encodeURIComponent(backup.name)}`">
              <LineIcon name="download" :size="16" />下载
            </a>
            <button class="button small ghost danger" type="button" @click="remove(backup)">删除</button>
          </div>
        </div>
      </div>
    </div>
  </section>
</template>
