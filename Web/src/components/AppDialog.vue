<script setup lang="ts">
import { onMounted, onUnmounted } from 'vue'
import LineIcon from './LineIcon.vue'

// 盖在页面上的对话框：点外面、按 Esc、点右上角都是关掉（close）
defineProps<{ title: string; width?: number }>()
const emit = defineEmits<{ close: [] }>()

const onKey = (event: KeyboardEvent) => {
  if (event.key === 'Escape') emit('close')
}
onMounted(() => window.addEventListener('keydown', onKey))
onUnmounted(() => window.removeEventListener('keydown', onKey))
</script>

<template>
  <div class="scrim" @mousedown.self="emit('close')">
    <div class="dialog" role="dialog" :aria-label="title" :style="{ width: `${width ?? 360}px` }">
      <div class="head">
        <h2>{{ title }}</h2>
        <button class="icon-button" type="button" aria-label="关闭" @click="emit('close')">
          <LineIcon name="x" :size="16" />
        </button>
      </div>
      <slot />
    </div>
  </div>
</template>

<style scoped>
.scrim {
  position: fixed;
  inset: 0;
  z-index: 20;
  display: grid;
  place-items: center;
  padding: 16px;
  background: var(--scrim);
}

.dialog {
  max-width: 100%;
  padding: 20px 22px 22px;
  border-radius: 14px;
  background: var(--surface);
  box-shadow: var(--shadow);
}

.head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin: -4px -8px 16px 0;
}

.head h2 {
  font-size: 17px;
  font-weight: 600;
}
</style>
