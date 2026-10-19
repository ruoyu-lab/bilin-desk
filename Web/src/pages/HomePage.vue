<script setup lang="ts">
import { computed, ref } from 'vue'
import DeviceTile from '../components/DeviceTile.vue'
import LineIcon from '../components/LineIcon.vue'
import { connectById, myDevices, openLogin, recentDevices, state } from '../state'

// 概览：最上面按设备 ID 和临时密码连接；旁边没登录时请登录，登录了说说客户端（网页只能去连别人）；
// 下面是我的设备、最近连过的
const id = ref('')
const password = ref('')
const digits = computed(() => id.value.replace(/\D/g, ''))

const submit = () => {
  if (digits.value.length === 9 && password.value.trim()) connectById(digits.value)
}

// 输入时三位一组
const formatId = () => {
  id.value = digits.value.slice(0, 9).replace(/(\d{3})(?=\d)/g, '$1 ')
}

const mine = computed(myDevices)
const recent = computed(() => recentDevices().filter((d) => !d.mine).slice(0, 6))
</script>

<template>
  <div class="page">
    <h1>概览</h1>

    <div class="cards">
      <form class="card connect" @submit.prevent="submit">
        <h2>连接到电脑</h2>
        <p class="muted">输入对方电脑上比邻显示的设备 ID 和临时密码。</p>
        <div class="fields">
          <input v-model="id" class="field num" placeholder="设备 ID" inputmode="numeric" autocomplete="off" @input="formatId" />
          <input v-model="password" class="field" placeholder="临时密码" autocomplete="off" spellcheck="false" />
          <button class="button dark" type="submit" :disabled="digits.length !== 9 || !password.trim()">连接</button>
        </div>
      </form>

      <div v-if="!state.account" class="card side">
        <h2>登录比邻账号</h2>
        <p class="muted">同一个账号的电脑不用密码，点一下就连。</p>
        <button class="button" type="button" @click="openLogin">登录 / 注册</button>
      </div>
      <div v-else class="card side">
        <h2>让别人连这台电脑</h2>
        <p class="muted">网页版只能去连别的电脑。被别人连、更清晰流畅的画面，用客户端。</p>
        <a class="button" href="https://www.bilindesk.com/#download" target="_blank" rel="noopener">
          <LineIcon name="download" :size="15" />下载客户端
        </a>
      </div>
    </div>

    <template v-if="state.account">
      <div class="section">
        <h3>我的设备</h3>
        <span class="hint">同一个账号登录的电脑</span>
        <a href="#/devices" class="more">全部</a>
      </div>
      <div v-if="mine.length > 0" class="tiles">
        <DeviceTile v-for="d in mine" :key="d.id" :device="d" />
      </div>
      <p v-else class="empty muted">别的电脑上登录同一个账号，就会出现在这里。</p>
    </template>

    <div class="section">
      <h3>最近连过</h3>
      <a v-if="recent.length > 0" href="#/recent" class="more">全部</a>
    </div>
    <div v-if="recent.length > 0" class="tiles">
      <DeviceTile v-for="d in recent" :key="d.id" :device="d" show-last />
    </div>
    <p v-else class="empty muted">连过的电脑会出现在这里，下次不用再输设备 ID。</p>
  </div>
</template>

<style scoped>
.page {
  max-width: 1104px;
  margin: 0 auto;
  padding: 28px 32px 48px;
}

h1 {
  margin-bottom: 20px;
  font-size: 22px;
  font-weight: 600;
}

.cards {
  display: grid;
  grid-template-columns: minmax(0, 1.6fr) minmax(0, 1fr);
  gap: 14px;
}

.card {
  display: flex;
  flex-direction: column;
  gap: 6px;
  padding: 18px 20px 20px;
}

.card h2 {
  font-size: 15px;
  font-weight: 600;
}

.card p {
  font-size: 13px;
  line-height: 1.6;
}

.side .button {
  align-self: flex-start;
  margin-top: auto;
}

.side p {
  margin-bottom: 14px;
}

.fields {
  display: grid;
  grid-template-columns: minmax(0, 1.2fr) minmax(0, 1fr) auto;
  gap: 8px;
  margin-top: 10px;
}

.connect .field {
  background: var(--field-on-card);
}

.section {
  display: flex;
  align-items: baseline;
  gap: 10px;
  margin: 30px 0 12px;
}

.section h3 {
  font-size: 15px;
  font-weight: 600;
}

.hint {
  font-size: 12px;
  color: var(--text-secondary);
}

.more {
  margin-left: auto;
  font-size: 13px;
  color: var(--text-secondary);
}

.more:hover {
  color: var(--text);
}

.tiles {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(230px, 1fr));
  gap: 10px;
}

.empty {
  padding: 4px 2px;
  font-size: 13px;
}

@media (max-width: 900px) {
  .cards {
    grid-template-columns: minmax(0, 1fr);
  }
}

@media (max-width: 560px) {
  .page {
    padding: 22px 16px 40px;
  }
  .fields {
    grid-template-columns: minmax(0, 1fr);
  }
}
</style>
