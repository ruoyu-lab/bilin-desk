<script setup lang="ts">
import { ago, connect, displayId, wake, wakeProblem, wakes, type Device } from '../state'
import LineIcon from './LineIcon.vue'

// 一台设备的小卡片（概览上）：系统的图标、名字、在不在线（我的设备才知道）、设备 ID 或者上次连接；点一下连接。
// 不在线的我的设备叫得醒时点一下叫醒它，叫得怎么样写在"离线"那里
const props = defineProps<{ device: Device; showLast?: boolean }>()
const offline = () => props.device.online === false
const attempt = () => wakes[props.device.id]
const wakeable = () => props.device.mine && offline() && !wakeProblem(props.device)
  && attempt()?.state !== 'asking' && attempt()?.state !== 'sent'
const title = () => !offline() ? `连接 ${props.device.name}`
  : attempt()?.note ?? (props.device.mine ? wakeProblem(props.device) || `唤醒 ${props.device.name}` : '不在线')
const stateText = () => ({ asking: '正在叫醒…', sent: '已叫醒，等它上线', failed: '没叫醒' })[attempt()?.state ?? 'failed']

function open() {
  if (wakeable()) void wake(props.device)
  else connect(props.device)
}
</script>

<template>
  <button class="tile" type="button" :class="{ offline: offline() }" :disabled="offline() && !wakeable()"
          :title="title()" @click="open">
    <span class="os" :class="props.device.system">
      <LineIcon :name="props.device.system === 'other' ? 'monitor' : props.device.system" :size="17" />
    </span>
    <span class="text">
      <span class="name">{{ props.device.name }}</span>
      <span class="state">
        <template v-if="props.device.online !== null">
          <span class="dot" :class="{ online: props.device.online }" />{{ props.device.online ? '在线'
            : attempt() ? stateText() : wakeable() ? '离线，点一下唤醒' : '离线' }} ·
        </template>
        <template v-if="props.showLast && props.device.lastConnected">{{ ago(props.device.lastConnected) }}</template>
        <span v-else class="num">{{ displayId(props.device.id) }}</span>
      </span>
    </span>
  </button>
</template>

<style scoped>
.tile {
  display: flex;
  align-items: center;
  gap: 12px;
  width: 100%;
  min-width: 0;
  padding: 12px 14px;
  border: 0;
  border-radius: var(--card-radius);
  background: var(--card);
  text-align: left;
  cursor: pointer;
  transition: background 0.15s;
}

.tile:hover:not(:disabled) {
  background: var(--card-hover);
}

.tile:disabled {
  cursor: default;
}

.tile.offline .os {
  opacity: 0.55;
}

.text {
  display: flex;
  flex-direction: column;
  gap: 3px;
  min-width: 0;
}

.name {
  overflow: hidden;
  font-size: 14px;
  font-weight: 500;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.state {
  display: flex;
  align-items: center;
  gap: 6px;
  font-size: 12px;
  color: var(--text-secondary);
}
</style>
