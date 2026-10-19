<script setup lang="ts">
import { computed } from 'vue'
import DeviceList from '../components/DeviceList.vue'
import { myDevices, openLogin, state } from '../state'

// 我的设备：同一个账号登录的电脑，在线的点"连接"就连，不用密码。收藏的排在前面
const devices = computed(() => [...myDevices()].sort((a, b) => Number(b.favorite) - Number(a.favorite)
  || Number(b.online) - Number(a.online)))
</script>

<template>
  <div class="page">
    <h1>我的设备</h1>
    <p class="intro muted">同一个账号登录的电脑，不用密码就能连。电脑睡眠、关机时网页叫不醒，要用同一个局域网里的客户端。</p>
    <DeviceList v-if="devices.length > 0" :devices="devices" />
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
