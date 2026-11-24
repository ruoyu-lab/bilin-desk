<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue'
import { api, type ConnectionRecord, type ConnectionSummary, type Count } from '../api'
import LineIcon from '../components/LineIcon.vue'
import type { IconName } from '../icons'
import { duration, shortDay, span, time } from '../format'

// 连接：程序发来的连接统计（docs/connection-stats.md）。上面是最近 7 天、30 天的汇总，下面是一条条的记录
const days = ref<7 | 30>(7)
const summary = ref<ConnectionSummary | null>(null)
const records = ref<ConnectionRecord[] | null>(null)
const onlyFailed = ref(false)
const deviceFilter = ref('')
const loading = ref(false)

async function loadSummary() {
  summary.value = await api<ConnectionSummary>('GET', `connections/summary?days=${days.value}`)
}

async function loadRecords() {
  const query = new URLSearchParams()
  if (onlyFailed.value) query.set('result', 'failed')
  const device = deviceFilter.value.replace(/\D/g, '')
  if (device.length === 9) query.set('deviceId', device)
  records.value = (await api<{ connections: ConnectionRecord[] }>('GET', `connections?${query}`)).connections
}

async function load() {
  loading.value = true
  try {
    await Promise.all([loadSummary(), loadRecords()])
  } catch {
    // 没登录时 api 已经回到登录页
  } finally {
    loading.value = false
  }
}
onMounted(load)
watch(days, () => void loadSummary().catch(() => {}))
watch(onlyFailed, () => void loadRecords().catch(() => {}))

// 筛一台设备：填满 9 位、清空时才去取
function filterDevice() {
  const digits = deviceFilter.value.replace(/\D/g, '')
  if (digits.length === 9 || digits.length === 0) void loadRecords().catch(() => {})
}

function percent(part: number, whole: number): string {
  return whole > 0 ? `${Math.round((part / whole) * 100)}%` : '—'
}

const kpis = computed<Array<{ icon: IconName; label: string; value: string; note: string }>>(() => {
  const s = summary.value
  if (!s) return []
  return [
    { icon: 'radar', label: '去连的次数', value: String(s.attempts), note: `${s.viewers} 台设备发起` },
    { icon: 'wifi', label: '连上了', value: percent(s.connected, s.attempts), note: `${s.connected} 次` },
    {
      icon: 'clock',
      label: '连上要多久',
      value: s.connectMsMedian === null ? '—' : span(s.connectMsMedian),
      note: s.connectMsP90 === null ? '中位数' : `中位数 · 九成在 ${span(s.connectMsP90)} 内`,
    },
    {
      icon: 'people',
      label: '一共用了',
      value: s.durationSTotal > 0 ? duration(s.durationSTotal) : '—',
      note: s.durationSMedian === null ? '' : `一次通常 ${span(s.durationSMedian * 1000)}`,
    },
  ]
})

// 每天：灰的是去连的次数，黑的是其中连上了的
const bars = computed(() => {
  const daily = summary.value?.daily ?? []
  const most = Math.max(1, ...daily.map((d) => d.attempts))
  return daily.map((d) => ({
    day: shortDay(d.day),
    attempts: d.attempts,
    connected: d.connected,
    height: (d.attempts / most) * 100,
    share: d.attempts > 0 ? (d.connected / d.attempts) * 100 : 0,
  }))
})

const pathNames: Record<string, string> = { lan: '局域网直连', direct: '跨网络直连', relay: '中继' }
const routeNames: Record<string, string> = { direct: '直连', relay: '中继（UDP）', relayTls: '中继（TLS 443）' }
const accessNames: Record<string, string> = { device: '我的设备', password: '临时密码', lan: '按局域网地址' }

// 一组数目画成横条：名字、条、次数和占比
function shares(counts: Count[], names: Record<string, string> = {}) {
  const total = counts.reduce((sum, c) => sum + c.count, 0)
  return counts.map((c) => ({ name: names[c.key] ?? c.key, count: c.count, share: total > 0 ? c.count / total : 0 }))
}

const breakdowns = computed(() => {
  const s = summary.value
  if (!s) return []
  return [
    { title: '连上时走的路', hint: '连上了的', items: shares(s.paths, pathNames) },
    { title: '最后用上的方式', hint: '直连不通才试中继，UDP 不通才试 TLS', items: shares(s.routes, routeNames) },
    { title: '没连上的原因', hint: '程序告诉用户的那句话', items: shares(s.failures) },
    { title: '连上以后断开的原因', hint: '用户自己关掉的不算', items: shares(s.endings) },
    { title: '谁连谁', hint: '去连的 → 被连的', items: shares(s.platforms) },
    { title: '怎么连的', hint: '', items: shares(s.access, accessNames) },
    { title: '编码格式', hint: '连上了的', items: shares(s.codecs) },
  ]
})

const usage = computed(() => {
  const s = summary.value
  if (!s) return []
  return [
    { label: '控制过对方', value: s.control },
    { label: '等过对方点允许', value: s.approval },
    { label: '中途断线重连过', value: s.reconnected },
    { label: '还没连上就关了', value: s.cancelled },
  ].map((u) => ({ ...u, share: percent(u.value, u.label === '还没连上就关了' ? s.attempts + s.cancelled : s.connected) }))
})

const resultNames: Record<ConnectionRecord['result'], string> = { connected: '连上了', failed: '没连上', cancelled: '关掉了' }

function deviceText(device: ConnectionRecord['viewer']): string {
  if (!device.deviceId) return '局域网地址'
  return [device.deviceId.replace(/(\d{3})(\d{3})(\d{3})/, '$1 $2 $3'), device.platform, device.version]
    .filter(Boolean)
    .join(' · ')
}

function routeText(r: ConnectionRecord): string {
  return r.routes.map((route) => routeNames[route] ?? route).join(' → ')
}

function qualityText(r: ConnectionRecord): string {
  if (r.result !== 'connected') return ''
  return [
    r.codec,
    r.resolution,
    r.fps ? `${Math.round(r.fps)} 帧` : '',
    r.mbps ? `${r.mbps.toFixed(1)} Mbit/s` : '',
    r.latencyMs ? `延迟 ${Math.round(r.latencyMs)} ms` : '',
  ]
    .filter(Boolean)
    .join(' · ')
}
</script>

<template>
  <section class="page">
    <div class="page-head">
      <div>
        <h1>连接</h1>
        <p>程序每次去连别的设备后报一条：怎么连的、连没连上、用了多久。不含画面、输入、对话、文件和地址，保留 90 天。</p>
      </div>
      <div class="actions">
        <div class="filters">
          <button type="button" :class="{ current: days === 7 }" @click="days = 7">7 天</button>
          <button type="button" :class="{ current: days === 30 }" @click="days = 30">30 天</button>
        </div>
        <button class="button" type="button" :disabled="loading" @click="load">
          <LineIcon name="refresh" :size="16" />{{ loading ? '正在读取…' : '刷新' }}
        </button>
      </div>
    </div>

    <template v-if="summary">
      <div class="kpis">
        <div v-for="k in kpis" :key="k.label" class="card kpi">
          <div class="tile"><LineIcon :name="k.icon" /></div>
          <div class="kpi-text">
            <div class="value num">{{ k.value }}</div>
            <div class="label">{{ k.label }}</div>
            <div v-if="k.note" class="note">{{ k.note }}</div>
          </div>
        </div>
      </div>

      <div class="card">
        <div class="card-head">
          <h2>每天</h2>
          <span class="hint">灰的是去连的次数，黑的是其中连上了的；按服务器的日期</span>
        </div>
        <div class="bars" :class="{ dense: bars.length > 7 }">
          <div v-for="b in bars" :key="b.day" class="bar-column" :title="`${b.day}：去连 ${b.attempts} 次，连上 ${b.connected} 次`">
            <div class="bar-value num">{{ b.attempts || '' }}</div>
            <div class="bar-track">
              <div class="bar" :style="{ height: `${b.height}%` }">
                <div class="bar-done" :style="{ height: `${b.share}%` }"></div>
              </div>
            </div>
            <div class="bar-day num">{{ b.day }}</div>
          </div>
        </div>
      </div>

      <div class="groups">
        <div v-for="g in breakdowns" :key="g.title" class="card">
          <div class="card-head">
            <h2>{{ g.title }}</h2>
            <span v-if="g.hint" class="hint">{{ g.hint }}</span>
          </div>
          <p v-if="g.items.length === 0" class="empty">还没有</p>
          <div v-for="item in g.items" :key="item.name" class="share">
            <span class="name" :title="item.name">{{ item.name }}</span>
            <div class="track thin"><div :style="{ width: `${item.share * 100}%` }"></div></div>
            <span class="count num">{{ item.count }} · {{ Math.round(item.share * 100) }}%</span>
          </div>
        </div>
        <div class="card">
          <div class="card-head"><h2>连着的时候</h2><span class="hint">占连上了的</span></div>
          <div v-for="u in usage" :key="u.label" class="share">
            <span class="name">{{ u.label }}</span>
            <span class="count num wide">{{ u.value }} 次 · {{ u.share }}</span>
          </div>
        </div>
      </div>

      <div class="card">
        <div class="card-head"><h2>各版本连上的比例</h2><span class="hint">按去连的一方的版本</span></div>
        <p v-if="summary.versions.length === 0" class="empty">还没有</p>
        <div v-for="v in summary.versions" :key="v.version" class="share version">
          <span class="name">{{ v.version }}</span>
          <div class="track thin"><div :style="{ width: `${v.attempts > 0 ? (v.connected / v.attempts) * 100 : 0}%` }"></div></div>
          <span class="count num">{{ v.connected }} / {{ v.attempts }} · {{ percent(v.connected, v.attempts) }}</span>
        </div>
      </div>
    </template>

    <div class="card">
      <div class="card-head records-head">
        <h2>记录</h2>
        <div class="record-filters">
          <input
            v-model="deviceFilter"
            class="field"
            inputmode="numeric"
            placeholder="只看一台设备（9 位 ID）"
            @input="filterDevice"
          />
          <div class="filters">
            <button type="button" :class="{ current: !onlyFailed }" @click="onlyFailed = false">全部</button>
            <button type="button" :class="{ current: onlyFailed }" @click="onlyFailed = true">没连上的</button>
          </div>
        </div>
      </div>
      <p v-if="records === null" class="empty">正在读取…</p>
      <p v-else-if="records.length === 0" class="empty">还没有记录</p>
      <div v-else class="rows">
        <div v-for="r in records" :key="r.id" class="row record">
          <span class="result" :class="r.result">{{ resultNames[r.result] }}</span>
          <div class="main">
            <div class="who">
              {{ deviceText(r.viewer) }}<span class="arrow">→</span>{{ deviceText(r.peer) }}
              <span class="tag light">{{ r.peer.deviceId ? accessNames[r.access] : accessNames.lan }}</span>
            </div>
            <div class="meta">
              {{ routeText(r) }}
              <template v-if="r.result === 'connected'">
                · {{ pathNames[r.path] ?? r.path }} · {{ span(r.connectMs) }}连上 · 连了 {{ span(r.durationS * 1000) }}
                <template v-if="r.reconnects"> · 重连 {{ r.reconnects }} 次</template>
                <template v-if="r.control"> · 控制过</template>
              </template>
            </div>
            <div v-if="qualityText(r)" class="meta">{{ qualityText(r) }}</div>
            <div v-if="r.reason" class="reason" :class="{ bad: r.result === 'failed' }">{{ r.reason }}</div>
          </div>
          <div class="when num">{{ time(r.createdAt) }}</div>
        </div>
      </div>
    </div>
  </section>
</template>

<style scoped>
.actions {
  display: flex;
  align-items: center;
  gap: 10px;
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

.kpis {
  display: grid;
  grid-template-columns: repeat(4, minmax(0, 1fr));
  gap: 16px;
}

@media (max-width: 1100px) {
  .kpis {
    grid-template-columns: repeat(2, minmax(0, 1fr));
  }
}

/* 分项的卡片两列（三列时名字太挤） */
.groups {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  align-items: start;
  gap: 16px;
}

@media (max-width: 760px) {
  .groups {
    grid-template-columns: minmax(0, 1fr);
  }
}

.kpi {
  display: flex;
  align-items: center;
  gap: 14px;
  padding: 18px 20px;
}

.kpi-text {
  min-width: 0;
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

.kpi .note {
  margin-top: 2px;
  overflow: hidden;
  font-size: 12px;
  color: var(--text-muted);
  text-overflow: ellipsis;
  white-space: nowrap;
}

.bars {
  display: flex;
  gap: 6px;
}

.bars.dense {
  gap: 3px;
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
  height: 96px;
  border-radius: 4px;
  background: var(--fill);
}

.bar {
  display: flex;
  align-items: flex-end;
  width: 100%;
  overflow: hidden;
  border-radius: 4px;
  background: #c3c7ce;
}

.bar-done {
  width: 100%;
  background: var(--text);
}

.bar-day {
  font-size: 11px;
  color: var(--text-muted);
}

.dense .bar-day:not(:nth-last-child(7n + 1)) {
  visibility: hidden;
}

.dense .bar-value {
  font-size: 10px;
}

.share {
  display: grid;
  grid-template-columns: minmax(0, 2fr) minmax(40px, 1fr) 76px;
  align-items: center;
  gap: 14px;
  padding: 6px 0;
}

.share .name {
  overflow: hidden;
  font-size: 14px;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.share .count {
  text-align: right;
  font-size: 13px;
  color: var(--text-secondary);
}

.share.version {
  grid-template-columns: minmax(0, 1fr) minmax(40px, 2fr) 120px;
}

.share .count.wide {
  grid-column: 2 / 4;
}

.track.thin {
  height: 6px;
}

.records-head {
  flex-wrap: wrap;
  gap: 12px;
}

.record-filters {
  display: flex;
  align-items: center;
  gap: 10px;
  margin-left: auto;
}

.record-filters .field {
  width: 200px;
  height: 34px;
  padding: 0 12px;
  border-radius: 8px;
  background: var(--bg);
  font-size: 13px;
}

.record {
  align-items: flex-start;
}

.result {
  flex-shrink: 0;
  width: 52px;
  padding-top: 1px;
  font-size: 13px;
  font-weight: 600;
}

.result.failed {
  color: var(--danger);
}

.result.cancelled {
  color: var(--text-muted);
  font-weight: 500;
}

.who {
  font-size: 14px;
  font-weight: 500;
  overflow-wrap: anywhere;
}

.arrow {
  margin: 0 8px;
  color: var(--text-muted);
}

.reason {
  margin-top: 4px;
  font-size: 13px;
  color: var(--text-secondary);
}

.reason.bad {
  color: var(--danger);
}

.when {
  flex-shrink: 0;
  font-size: 12px;
  color: var(--text-muted);
}

@media (max-width: 720px) {
  .record {
    flex-wrap: wrap;
  }
  .when {
    width: 100%;
    padding-left: 66px;
  }
  .record-filters {
    margin-left: 0;
    flex-wrap: wrap;
  }
}
</style>
