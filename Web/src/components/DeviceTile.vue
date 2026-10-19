<script setup lang="ts">
import { ago, connect, displayId, type Device } from '../state'
import LineIcon from './LineIcon.vue'

// 一台设备的小卡片（概览上）：系统的图标、名字、在不在线（我的设备才知道）、设备 ID 或者上次连接；点一下连接
const props = defineProps<{ device: Device; showLast?: boolean }>()
const offline = () => props.device.online === false
</script>

<template>
  <button class="tile" type="button" :class="{ offline: offline() }" :disabled="offline()"
          :title="offline() ? '不在线' : `连接 ${props.device.name}`" @click="connect(props.device)">
    <span class="os" :class="props.device.system">
      <LineIcon :name="props.device.system === 'other' ? 'monitor' : props.device.system" :size="17" />
    </span>
    <span class="text">
      <span class="name">{{ props.device.name }}</span>
      <span class="state">
        <template v-if="props.device.online !== null">
          <span class="dot" :class="{ online: props.device.online }" />{{ props.device.online ? '在线' : '离线' }} ·
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
