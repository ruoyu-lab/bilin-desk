<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref } from 'vue'
import { api, ApiError, platformName, type Status } from '../api'
import LineIcon from '../components/LineIcon.vue'
import type { IconName } from '../icons'
import { ago, bytes, daysUntil, duration, shortDay, time } from '../format'
import { toast } from '../toasts'

// 概览：服务器和服务端在不在正常运行。只有数目，不看任何一个账号、设备。每 30 秒刷新
const status = ref<Status | null>(null)

async function load() {
  try {
    status.value = await api<Status>('GET', 'status')
  } catch {
    // 没登录时 api 已经回到登录页；别的错下次刷新再看
  }
}

let timer = 0
onMounted(() => {
  void load()
  timer = window.setInterval(load, 30_000)
})
onUnmounted(() => window.clearInterval(timer))

const kpis = computed<Array<{ icon: IconName; label: string; value: string }>>(() => {
  const s = status.value
  if (!s) return []
  return [
    { icon: 'wifi', label: '在线的设备', value: String(s.devices.online) },
    { icon: 'monitor', label: '登记过的设备', value: String(s.devices.registered) },
    { icon: 'user', label: '账号', value: String(s.accounts) },
    { icon: 'database', label: '数据库', value: bytes(s.databaseBytes) },
  ]
})

// 证书、备份、邮件：好不好（null：开发环境，不算好坏），一句话说是什么情况，下面一行细节
type Health = { icon: IconName; title: string; ok: boolean | null; text: string; detail: string }

const CERTIFICATE_WARN_DAYS = 14
// 每天凌晨备份一次：超过一天多还没有新的，就是没备份成
const BACKUP_STALE_MS = 26 * 3600 * 1000
const DAY_MS = 86_400_000

const health = computed<Health[]>(() => {
  const s = status.value
  if (!s) return []
  const certificate: Health = !s.certificate
    ? { icon: 'shield', title: '证书', ok: null, text: '开发环境没有证书', detail: '' }
    : !s.certificate.validTo
      ? { icon: 'shield', title: '证书', ok: false, text: '问不到证书', detail: s.certificate.host }
      : {
          icon: 'shield',
          title: '证书',
          ok: daysUntil(s.certificate.validTo) >= CERTIFICATE_WARN_DAYS,
          text: `还有 ${daysUntil(s.certificate.validTo)} 天到期`,
          detail: `${s.certificate.host} · 到期前一个月自动续期`,
        }
  const backup: Health = !s.backup
    ? { icon: 'database', title: '备份', ok: false, text: '还没有备份', detail: '每天凌晨 4 点自动备份' }
    : {
        icon: 'database',
        title: '备份',
        ok: Date.now() - new Date(s.backup.createdAt).getTime() < BACKUP_STALE_MS,
        text: `上次备份：${ago(s.backup.createdAt)}`,
        detail: `${time(s.backup.createdAt)} · ${bytes(s.backup.size)}`,
      }
  const failure = s.mail.lastFailure
  const mail: Health =
    s.mail.mode === 'log'
      ? { icon: 'mail', title: '验证码邮件', ok: null, text: '开发环境：写进日志', detail: '' }
      : s.mail.mode === 'none'
        ? { icon: 'mail', title: '验证码邮件', ok: false, text: '还没配 Resend 的密钥', detail: '运行 Server/deploy/set-mail-key.sh' }
        : {
            icon: 'mail',
            title: '验证码邮件',
            ok: !failure || Date.now() - new Date(failure.at).getTime() > DAY_MS,
            text: `今天发了 ${s.mail.today} 封，一天最多 ${s.mail.limit} 封`,
            detail: failure ? `最近一次没发出去（${ago(failure.at)}）：${failure.reason}` : '经 Resend 发出',
          }
  return [certificate, backup, mail]
})

const meters = computed(() => {
  const s = status.value
  if (!s) return []
  return [
    { label: '内存', used: s.memory.totalBytes - s.memory.availableBytes, total: s.memory.totalBytes },
    { label: '磁盘', used: s.disk.totalBytes - s.disk.availableBytes, total: s.disk.totalBytes },
  ].map((m) => ({ ...m, share: m.total > 0 ? m.used / m.total : 0 }))
})

const details = computed(() => {
  const s = status.value
  if (!s) return []
  return [
    { label: '系统', value: s.system.os || '—' },
    { label: '内核', value: s.system.kernel },
    { label: 'CPU', value: `${s.system.cpus} 核 · 负载 ${s.system.load.join(' / ')}` },
    { label: '软件', value: `Node ${s.system.node.replace(/^v/, '')} · MongoDB ${s.system.mongodb} · Redis ${s.system.redis}` },
    { label: '已运行', value: duration(s.uptimeS) },
  ]
})

const serviceNames: Record<string, string> = {
  server: '服务端',
  mongod: 'MongoDB',
  redis: 'Redis',
  nginx: 'Nginx',
  coturn: '中继（coturn）',
}

// 最近 7 天：每个数一张小柱状图，今天的是黑的；同时在线看最多，别的看合计
type Series = 'devices' | 'accounts' | 'connections' | 'peakOnline'
const series: Array<{ key: Series; label: string; summary: 'sum' | 'max' }> = [
  { key: 'connections', label: '发起的连接', summary: 'sum' },
  { key: 'peakOnline', label: '同时在线最多', summary: 'max' },
  { key: 'devices', label: '新设备', summary: 'sum' },
  { key: 'accounts', label: '新账号', summary: 'sum' },
]

const charts = computed(() => {
  const days = status.value?.trends ?? []
  return series.map((s) => {
    const values = days.map((d) => d[s.key])
    const most = Math.max(1, ...values)
    return {
      ...s,
      total: s.summary === 'sum' ? values.reduce((a, b) => a + b, 0) : Math.max(0, ...values),
      bars: days.map((d, i) => ({ day: shortDay(d.day), value: values[i] ?? 0, height: ((values[i] ?? 0) / most) * 100 })),
    }
  })
})

// 发测试邮件（"验证码邮件"那张卡片里）：发出去了提示一下，没发出去说原因
const testTo = ref('')
const testing = ref(false)
const testError = ref('')

async function sendTest() {
  testing.value = true
  testError.value = ''
  try {
    await api('POST', 'mail/test', { to: testTo.value })
    toast(`测试邮件发给了 ${testTo.value.trim()}，去收件箱看看`)
    await load()
  } catch (e) {
    if (e instanceof ApiError && e.code === 'signedOut') return
    testError.value =
      e instanceof ApiError && e.code === 'email' ? '邮箱格式不对'
      : e instanceof ApiError && e.code === 'tooMany' ? '一小时最多发 10 封测试邮件，过一会儿再试'
      : e instanceof ApiError && e.detail ? `没发出去：${e.detail}`
      : '没发出去，看看出错日志'
    await load()
  } finally {
    testing.value = false
  }
}

const totalDevices = computed(() => (status.value?.versions ?? []).reduce((sum, v) => sum + v.devices, 0))
</script>

<template>
  <section class="page">
    <div class="page-head">
      <div>
        <h1>概览</h1>
        <p>只有数目，不看任何账号、设备的内容。每 30 秒刷新。</p>
      </div>
    </div>

    <template v-if="status">
      <div class="kpis">
        <div v-for="k in kpis" :key="k.label" class="card kpi">
          <div class="tile"><LineIcon :name="k.icon" /></div>
          <div>
            <div class="value num">{{ k.value }}</div>
            <div class="label">{{ k.label }}</div>
          </div>
        </div>
      </div>

      <div class="health">
        <div v-for="h in health" :key="h.title" class="card check">
          <div class="check-head">
            <LineIcon :name="h.icon" :size="18" />
            <span>{{ h.title }}</span>
            <span class="dot" :class="{ bad: h.ok === false, idle: h.ok === null }"></span>
          </div>
          <div class="check-text" :class="{ bad: h.ok === false }">{{ h.text }}</div>
          <div v-if="h.detail" class="check-detail">{{ h.detail }}</div>
          <form v-if="h.icon === 'mail' && status.mail.mode !== 'none'" class="test" @submit.prevent="sendTest">
            <input v-model="testTo" class="field" type="email" placeholder="发一封测试邮件到…" />
            <button class="button small primary" type="submit" :disabled="testing || testTo.trim().length === 0">
              {{ testing ? '正在发…' : '发送' }}
            </button>
          </form>
          <div v-if="h.icon === 'mail' && testError" class="error test-error">{{ testError }}</div>
        </div>
      </div>

      <div class="card">
        <div class="card-head">
          <h2>最近 7 天</h2>
          <span class="hint">按服务器的日期</span>
        </div>
        <div class="charts">
          <div v-for="c in charts" :key="c.key" class="chart">
            <div class="chart-label">{{ c.label }}</div>
            <div class="chart-total num">{{ c.total }}</div>
            <div class="bars">
              <div v-for="(b, i) in c.bars" :key="b.day" class="bar-column">
                <div class="bar-value num">{{ b.value || '' }}</div>
                <div class="bar-track">
                  <div class="bar" :class="{ today: i === c.bars.length - 1 }" :style="{ height: `${b.height}%` }"></div>
                </div>
                <div class="bar-day num">{{ b.day }}</div>
              </div>
            </div>
          </div>
        </div>
      </div>

      <div class="grid two">
        <div class="card">
          <div class="card-head"><h2>服务器</h2></div>
          <div v-for="m in meters" :key="m.label" class="meter">
            <div class="top">
              <span>{{ m.label }}</span>
              <span class="num">{{ bytes(m.used) }} / {{ bytes(m.total) }}　{{ Math.round(m.share * 100) }}%</span>
            </div>
            <div class="track"><div :class="{ warn: m.share > 0.85 }" :style="{ width: `${m.share * 100}%` }"></div></div>
          </div>
          <dl class="details">
            <template v-for="d in details" :key="d.label">
              <dt>{{ d.label }}</dt>
              <dd class="num">{{ d.value }}</dd>
            </template>
          </dl>
        </div>
        <div class="card">
          <div class="card-head"><h2>服务</h2></div>
          <div class="rows">
            <div v-for="(up, name) in status.services" :key="name" class="row service">
              <span class="dot" :class="{ bad: !up }"></span>
              <span class="main">{{ serviceNames[name] ?? name }}</span>
              <span class="state" :class="{ bad: !up }">{{ up ? '运行中' : '没在运行' }}</span>
            </div>
          </div>
        </div>
      </div>

      <div class="card">
        <div class="card-head">
          <h2>各版本的设备</h2>
          <span class="hint">最近 30 天上过线的</span>
        </div>
        <div v-if="status.versions.length === 0" class="empty">最近 30 天还没有设备上线</div>
        <div v-for="v in status.versions" :key="`${v.platform} ${v.version}`" class="version">
          <span class="name">{{ platformName(v.platform) }} {{ v.version }}</span>
          <div class="track thin"><div :style="{ width: `${(v.devices / totalDevices) * 100}%` }"></div></div>
          <span class="count num">{{ v.devices }} 台 · {{ Math.round((v.devices / totalDevices) * 100) }}%</span>
        </div>
      </div>
    </template>
    <p v-else class="empty">正在读取…</p>
  </section>
</template>

<style scoped>
/* 宽屏：数字一行 4 个、证书那一排 3 个；窄了数字两行、证书那一排竖着 */
.kpis {
  display: grid;
  grid-template-columns: repeat(4, minmax(0, 1fr));
  gap: 16px;
}

.health {
  display: grid;
  grid-template-columns: repeat(3, minmax(0, 1fr));
  gap: 16px;
}

@media (max-width: 1100px) {
  .kpis {
    grid-template-columns: repeat(2, minmax(0, 1fr));
  }
  .health {
    grid-template-columns: 1fr;
  }
}

.kpi {
  display: flex;
  align-items: center;
  gap: 14px;
  padding: 18px 20px;
}

.kpi .value {
  font-size: 24px;
  font-weight: 800;
  line-height: 1.15;
}

.kpi .label {
  margin-top: 2px;
  font-size: 13px;
  color: var(--text-secondary);
}

.check {
  min-width: 0;
  padding: 18px 20px;
}

.check-head {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 13px;
  color: var(--text-secondary);
}

.check-head .dot {
  margin-left: auto;
}

.check-text {
  margin-top: 10px;
  font-size: 16px;
  font-weight: 600;
}

.check-text.bad {
  color: var(--danger);
}

.check-detail {
  margin-top: 4px;
  overflow: hidden;
  font-size: 12px;
  color: var(--text-muted);
  text-overflow: ellipsis;
  white-space: nowrap;
}

.test {
  display: flex;
  gap: 8px;
  margin-top: 14px;
}

.test .field {
  height: 34px;
  padding: 0 12px;
  border-radius: 8px;
  background: var(--bg);
  font-size: 13px;
}

.test .button {
  height: 34px;
}

.test-error {
  margin-top: 8px;
  font-size: 12px;
  overflow-wrap: anywhere;
}

.dot {
  flex-shrink: 0;
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background: var(--online);
}

.dot.bad {
  background: var(--danger);
}

.dot.idle {
  background: var(--offline);
}

/* 最近 7 天的小柱状图 */
.charts {
  display: grid;
  grid-template-columns: repeat(4, minmax(0, 1fr));
  gap: 28px;
}

@media (max-width: 1100px) {
  .charts {
    grid-template-columns: repeat(2, minmax(0, 1fr));
  }
}

.chart-label {
  font-size: 13px;
  color: var(--text-secondary);
}

.chart-total {
  margin-top: 2px;
  font-size: 22px;
  font-weight: 800;
}

.bars {
  display: flex;
  gap: 6px;
  margin-top: 12px;
}

.bar-column {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 4px;
  min-width: 0;
}

.bar-value {
  height: 14px;
  font-size: 11px;
  color: var(--text-muted);
}

.bar-track {
  display: flex;
  align-items: flex-end;
  width: 100%;
  height: 64px;
  border-radius: 4px;
  background: var(--fill);
}

.bar {
  width: 100%;
  border-radius: 4px;
  background: #c3c7ce;
}

.bar.today {
  background: var(--text);
}

.bar-day {
  font-size: 11px;
  color: var(--text-muted);
}

.meter + .meter {
  margin-top: 16px;
}

.meter .top {
  display: flex;
  justify-content: space-between;
  margin-bottom: 8px;
  font-size: 14px;
}

.meter .top span:last-child {
  color: var(--text-secondary);
}

.details {
  display: grid;
  grid-template-columns: auto 1fr;
  gap: 8px 16px;
  margin: 20px 0 0;
  padding-top: 16px;
  border-top: 1px solid var(--line);
  font-size: 13px;
}

.details dt {
  color: var(--text-muted);
}

.details dd {
  margin: 0;
  overflow-wrap: anywhere;
}

.service {
  font-size: 14px;
}

.state {
  font-size: 13px;
  color: var(--text-muted);
}

.state.bad {
  color: var(--danger);
  font-weight: 600;
}

.version {
  display: grid;
  grid-template-columns: 170px 1fr 92px;
  align-items: center;
  gap: 14px;
  padding: 6px 0;
}

.version .name {
  overflow: hidden;
  font-size: 14px;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.version .count {
  text-align: right;
  font-size: 13px;
  color: var(--text-secondary);
}

.track.thin {
  height: 6px;
}

@media (max-width: 720px) {
  .version {
    grid-template-columns: 120px 1fr 80px;
  }
}
</style>
