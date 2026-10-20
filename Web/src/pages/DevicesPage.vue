<script setup lang="ts">
import { computed } from 'vue'
import DeviceList from '../components/DeviceList.vue'
import { myDevices, openLogin, state } from '../state'

// 我的设备：同一个账号登录的电脑（服务器说的，手机、平板不能被连，不列），在线的点"连接"就连，不用密码
const devices = computed(() => [...myDevices()].sort((a, b) => Number(b.online) - Number(a.online)
  || Number(b.favorite) - Number(a.favorite)))
</script>

<template>
  <div class="page">
    <h1>我的设备</h1>
    <p class="intro muted">同一个账号登录的电脑，不用密码就能连。睡着了的电脑，可以请和它在同一个网络里、开着的另一台电脑帮忙叫醒。</p>
    <DeviceList v-if="devices.length > 0" :devices="devices" />
    <div v-else-if="!state.loaded" class="card empty"><p class="muted">正在读取…</p></div>
    <div v-else-if="!state.account" class="card empty">
      <p class="muted">登录比邻账号以后，同一个账号的电脑会出现在这里。</p>
      <button class="button dark" type="button" @click="openLogin">登录 / 注册</button>
    </div>
    <div v-else class="card empty">
      <p class="muted">别的电脑上登录同一个账号，就会出现在这里。</p>
    </div>
  </div>
</template>

<style scoped>
.page {
  max-width: 1104px;
  margin: 0 auto;
  padding: 28px 32px 48px;
}

h1 {
  font-size: 22px;
  font-weight: 600;
}

.intro {
  margin: 6px 0 18px;
  font-size: 13px;
  line-height: 1.6;
}

.empty {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 14px;
  padding: 40px 20px;
  font-size: 13px;
}

@media (max-width: 560px) {
  .page {
    padding: 22px 16px 40px;
  }
}
</style>
