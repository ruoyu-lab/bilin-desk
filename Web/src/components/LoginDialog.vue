<script setup lang="ts">
import { ref } from 'vue'
import { signIn } from '../state'
import AppDialog from './AppDialog.vue'

// 登录 / 注册（和程序里一样：注册要先往邮箱发验证码）。登录以后同一个账号的电脑不用密码就能连
const emit = defineEmits<{ close: [] }>()

const mode = ref<'login' | 'register'>('login')
const nickname = ref('')
const email = ref('')
const password = ref('')
const code = ref('')
const codeSent = ref(false)

const submit = () => {
  signIn(email.value.trim())
  emit('close')
}
</script>

<template>
  <AppDialog :title="mode === 'login' ? '登录比邻' : '注册比邻账号'" @close="emit('close')">
    <p class="intro muted">登录以后，同一个账号的电脑不用密码就能连。</p>
    <form class="form" @submit.prevent="submit">
      <input v-if="mode === 'register'" v-model="nickname" class="field" placeholder="昵称" maxlength="32" />
      <input v-model="email" class="field" type="email" placeholder="邮箱" autocomplete="username" />
      <div v-if="mode === 'register'" class="code-row">
        <input v-model="code" class="field" placeholder="邮箱收到的验证码" inputmode="numeric" maxlength="6" />
        <button class="button" type="button" :disabled="!email.trim()" @click="codeSent = true">
          {{ codeSent ? '重新发送' : '发送验证码' }}
        </button>
      </div>
      <input v-model="password" class="field" type="password" :placeholder="mode === 'login' ? '密码' : '密码（至少 8 位）'"
             :autocomplete="mode === 'login' ? 'current-password' : 'new-password'" />
      <button class="button dark large" type="submit" :disabled="!email.trim() || !password">
        {{ mode === 'login' ? '登录' : '注册并登录' }}
      </button>
    </form>
    <div class="switch">
      <template v-if="mode === 'login'">
        还没有账号？<a href="#" @click.prevent="mode = 'register'">注册</a>
      </template>
      <template v-else>
        已经有账号了？<a href="#" @click.prevent="mode = 'login'">登录</a>
      </template>
    </div>
  </AppDialog>
</template>

<style scoped>
.intro {
  margin: -6px 0 16px;
  font-size: 13px;
}

.form {
  display: flex;
  flex-direction: column;
  gap: 10px;
}

.form .button {
  margin-top: 6px;
}

.code-row {
  display: flex;
  gap: 8px;
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
</style>
