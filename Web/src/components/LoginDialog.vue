<script setup lang="ts">
import { computed, onUnmounted, ref } from 'vue'
import { ApiError, sendCode } from '../api'
import { registerAccount, resetPassword, signIn } from '../state'
import AppDialog from './AppDialog.vue'

// 登录、注册、找回密码（和程序里一样：注册、找回密码要先往邮箱发验证码）。登录以后同一个账号的电脑不用密码就能连
const emit = defineEmits<{ close: [] }>()

type Mode = 'login' | 'register' | 'reset'
const mode = ref<Mode>('login')
const nickname = ref('')
const email = ref('')
const password = ref('')
const code = ref('')
const busy = ref(false)
const error = ref('')
// 验证码发了以后多少秒才能再发（服务端一分钟一个）
const wait = ref(0)
let timer = 0
onUnmounted(() => window.clearInterval(timer))

const titles: Record<Mode, string> = { login: '登录比邻', register: '注册比邻账号', reset: '找回密码' }
const ready = computed(() => email.value.trim().length > 0 && (mode.value === 'login'
  ? password.value.length > 0
  : password.value.length >= 8 && /^\d{6}$/.test(code.value.trim()) && (mode.value === 'reset' || nickname.value.trim().length > 0)))

function switchTo(next: Mode) {
  mode.value = next
  error.value = ''
  code.value = ''
}

async function run(work: () => Promise<void>) {
  busy.value = true
  error.value = ''
  try {
    await work()
  } catch (e) {
    error.value = e instanceof ApiError ? e.message : '出了点问题，过一会儿再试'
  } finally {
    busy.value = false
  }
}

function sendEmailCode() {
  void run(async () => {
    await sendCode(email.value.trim(), mode.value === 'register' ? 'register' : 'resetPassword')
    wait.value = 60
    window.clearInterval(timer)
    timer = window.setInterval(() => {
      if (--wait.value <= 0) window.clearInterval(timer)
    }, 1000)
  })
}

function submit() {
  if (!ready.value) return
  void run(async () => {
    const address = email.value.trim()
    if (mode.value === 'login') await signIn(address, password.value)
    else if (mode.value === 'register') await registerAccount(nickname.value.trim(), address, password.value, code.value.trim())
    else await resetPassword(address, code.value.trim(), password.value)
    emit('close')
  })
}
</script>

<template>
  <AppDialog :title="titles[mode]" @close="emit('close')">
    <p class="intro muted">
      {{ mode === 'reset' ? '往注册时的邮箱发一个验证码，凭它设新密码。' : '登录以后，同一个账号的电脑不用密码就能连。' }}
    </p>
    <form class="form" @submit.prevent="submit">
      <input v-if="mode === 'register'" v-model="nickname" class="field" placeholder="昵称" maxlength="32" />
      <input v-model="email" class="field" type="email" placeholder="邮箱" autocomplete="username" />
      <div v-if="mode !== 'login'" class="code-row">
        <input v-model="code" class="field num" placeholder="邮箱收到的验证码" inputmode="numeric" maxlength="6"
               autocomplete="one-time-code" />
        <button class="button" type="button" :disabled="!email.trim() || wait > 0 || busy" @click="sendEmailCode">
          {{ wait > 0 ? `${wait} 秒后重发` : '发送验证码' }}
        </button>
      </div>
      <input v-model="password" class="field" type="password"
             :placeholder="mode === 'login' ? '密码' : mode === 'reset' ? '新密码（至少 8 位）' : '密码（至少 8 位）'"
             :autocomplete="mode === 'login' ? 'current-password' : 'new-password'" maxlength="128" />
      <p v-if="error" class="error">{{ error }}</p>
      <button class="button dark large" type="submit" :disabled="!ready || busy">
        {{ busy ? '请稍候…' : mode === 'login' ? '登录' : mode === 'register' ? '注册并登录' : '设新密码并登录' }}
      </button>
    </form>
    <div class="switch">
      <template v-if="mode === 'login'">
        还没有账号？<a href="#" @click.prevent="switchTo('register')">注册</a>
        <span class="dot-sep">·</span>
        <a href="#" @click.prevent="switchTo('reset')">忘记密码</a>
      </template>
      <template v-else>
        想起来了？<a href="#" @click.prevent="switchTo('login')">登录</a>
      </template>
    </div>
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

.form .button.large {
  margin-top: 6px;
}

.code-row {
  display: flex;
  gap: 8px;
}

.code-row .button {
  min-width: 104px;
}

.switch {
  margin-top: 16px;
  font-size: 13px;
  text-align: center;
  color: var(--text-secondary);
}

.switch a {
  color: var(--text);
  font-weight: 500;
}

.dot-sep {
  margin: 0 8px;
}
</style>
