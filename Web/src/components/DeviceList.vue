<script setup lang="ts">
import { ago, connect, displayId, toggleFavorite, wake, wakeProblem, wakes, type Device } from '../state'
import LineIcon from './LineIcon.vue'

// 一行一台设备（我的设备、收藏、最近）：系统的图标、名字和设备 ID、在不在线（我的设备才知道）、上次连接，
// 右边收藏和连接；最近里能去掉一台（removable）。不在线的我的设备叫得醒时"连接"换成"唤醒"，叫得怎么样写在 ID 后面
const props = defineProps<{ devices: Device[]; removable?: boolean }>()
const emit = defineEmits<{ remove: [device: Device] }>()

const wakeable = (d: Device) => d.mine && d.online === false && (wakes[d.id] !== undefined || !wakeProblem(d))
const waking = (d: Device) => wakes[d.id]?.state === 'asking' || wakes[d.id]?.state === 'sent'
</script>

<template>
  <div class="list">
    <div v-for="d in props.devices" :key="d.id" class="row" :class="{ offline: d.online === false }">
      <span class="os" :class="d.system"><LineIcon :name="d.system === 'other' ? 'monitor' : d.system" :size="17" /></span>
      <div class="main">
        <div class="name">{{ d.name }}<span v-if="d.mine" class="tag">我的设备</span></div>
        <div class="meta">
          <span class="num">{{ displayId(d.id) }}</span>
          <span v-if="wakes[d.id]" :class="{ error: wakes[d.id]?.state === 'failed' }"> · {{ wakes[d.id]?.note }}</span>
        </div>
      </div>
      <div class="state">
        <template v-if="d.online !== null">
          <span class="dot" :class="{ online: d.online }" />{{ d.online ? '在线' : '离线' }}
        </template>
      </div>
      <div class="last">{{ d.lastConnected ? ago(d.lastConnected) : '没连过' }}</div>
      <div class="actions">
        <button class="icon-button" type="button" :class="{ favorite: d.favorite }"
                :aria-label="d.favorite ? '取消收藏' : '收藏'" :title="d.favorite ? '取消收藏' : '收藏'"
                @click="toggleFavorite(d)">
          <LineIcon name="star" :size="16" />
        </button>
        <button v-if="props.removable" class="icon-button" type="button" aria-label="从最近里去掉" title="从最近里去掉"
                @click="emit('remove', d)">
          <LineIcon name="x" :size="15" />
        </button>
        <button v-if="wakeable(d)" class="button" type="button" :disabled="waking(d)" @click="wake(d)">
          {{ wakes[d.id]?.state === 'asking' ? '正在叫醒…' : wakes[d.id]?.state === 'sent' ? '已叫醒' : '唤醒' }}
        </button>
        <button v-else class="button" type="button" :disabled="d.online === false"
                :title="d.mine && d.online === false ? wakeProblem(d) : ''" @click="connect(d)">连接</button>
      </div>
    </div>
  </div>
</template>

<style scoped>
.list {
  padding: 4px;
  border-radius: var(--card-radius);
  background: var(--card);
}

.row {
  display: grid;
  grid-template-columns: auto minmax(0, 1fr) 72px 90px auto;
  align-items: center;
  gap: 14px;
  padding: 10px 12px;
  border-radius: var(--radius);
  transition: background 0.15s;
}

.row + .row {
  box-shadow: 0 -1px 0 var(--border);
}

.row:hover {
  background: var(--hover);
  box-shadow: none;
}

.row:hover + .row {
  box-shadow: none;
}

.row.offline .os {
  opacity: 0.55;
}

.name {
  overflow: hidden;
  font-size: 14px;
  font-weight: 500;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.tag {
  margin-left: 8px;
  padding: 1px 7px;
  border-radius: 999px;
  background: var(--fill);
  font-size: 11px;
  font-weight: 400;
  color: var(--text-secondary);
  vertical-align: 1px;
}

.meta,
.last {
  font-size: 12px;
  color: var(--text-secondary);
}

.meta {
  overflow: hidden;
  margin-top: 2px;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.meta .error {
  font-size: inherit;
}

.state {
  display: flex;
  align-items: center;
  gap: 6px;
  font-size: 13px;
  color: var(--text-secondary);
}

.actions {
  display: flex;
  align-items: center;
  gap: 6px;
}

.icon-button.favorite {
  color: #ffb340;
}

@media (max-width: 760px) {
  .row {
    grid-template-columns: auto minmax(0, 1fr) auto;
  }
  .state,
  .last {
    display: none;
  }
}
</style>
