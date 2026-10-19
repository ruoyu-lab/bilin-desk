<script setup lang="ts">
import { onMounted, ref } from 'vue'
import { displayId, type Device } from '../state'
import AppDialog from './AppDialog.vue'

// 连不是我的设备：要对方屏幕上显示的临时密码
const props = defineProps<{ device: Device }>()
const emit = defineEmits<{ close: []; connect: [password: string] }>()

const password = ref('')
const input = ref<HTMLInputElement | null>(null)
onMounted(() => input.value?.focus())
</script>

<template>
  <AppDialog :title="`连接 ${props.device.name}`" @close="emit('close')">
    <p class="intro muted">输入对方电脑上比邻显示的临时密码（设备 ID {{ displayId(props.device.id) }}）。</p>
    <form class="form" @submit.prevent="emit('connect', password.trim())">
      <input ref="input" v-model="password" class="field" placeholder="临时密码" autocomplete="off" spellcheck="false" />
      <button class="button dark large" type="submit" :disabled="!password.trim()">连接</button>
    </form>
  </AppDialog>
</template>

<style scoped>
.intro {
  margin: -6px 0 16px;
  font-size: 13px;
  line-height: 1.6;
}

.form {
  display: flex;
  flex-direction: column;
  gap: 10px;
}
</style>
