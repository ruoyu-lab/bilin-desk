<script setup lang="ts">
import { ref } from 'vue'
import { api, ApiError } from '../api'
import LogoMark from '@shared/LogoMark.vue'

const emit = defineEmits<{ signedIn: [] }>()

const password = ref('')
const error = ref('')
const busy = ref(false)

const errors: Record<string, string> = {
  wrong: '密码不对',
  tooMany: '试错太多次，过 15 分钟再试',
  noPassword: '还没设管理后台的密码：运行 Server/deploy/set-admin-password.sh',
}

async function login() {
  error.value = ''
  busy.value = true
  try {
    await api('POST', 'login', { password: password.value })
    password.value = ''
    emit('signedIn')
  } catch (e) {
    error.value = (e instanceof ApiError && e.code && errors[e.code]) || '登录没成，稍后再试'
  } finally {
    busy.value = false
  }
}
</script>

<template>
  <form class="login" @submit.prevent="login">
    <LogoMark :size="56" />
    <h1>比邻 · 管理后台</h1>
    <p class="lead">服务器的状态、发布新版本、数据库的备份</p>
    <input
      v-model="password"
      class="field"
      type="password"
      placeholder="密码"
      autocomplete="current-password"
      autofocus
    />
    <button class="button primary submit" type="submit" :disabled="busy || password.length === 0">
      {{ busy ? '请稍候…' : '登录' }}
    </button>
    <p class="error">{{ error }}</p>
  </form>
</template>

<style scoped>
.login {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 12px;
  width: 100%;
  max-width: 340px;
  margin: 0 auto;
  padding: 18vh 24px 48px;
  text-align: center;
}

h1 {
  margin-top: 12px;
  font-size: 24px;
  font-weight: 900;
}

.lead {
  margin-bottom: 14px;
  font-size: 14px;
  color: var(--text-secondary);
}

.submit {
  width: 100%;
  height: 42px;
}

.error {
  min-height: 20px;
}
</style>
