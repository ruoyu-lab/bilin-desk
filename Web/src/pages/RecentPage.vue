<script setup lang="ts">
import { computed } from 'vue'
import DeviceList from '../components/DeviceList.vue'
import { recentDevices } from '../state'

// 最近：这个浏览器里连过的电脑，新的在前（记在浏览器里，换个浏览器就没有）
const devices = computed(recentDevices)
</script>

<template>
  <div class="page">
    <h1>最近</h1>
    <p class="intro muted">在这个浏览器里连过的电脑。不是我的设备的，连的时候还要输入临时密码。</p>
    <DeviceList v-if="devices.length > 0" :devices="devices" />
    <div v-else class="card empty">
      <p class="muted">连过的电脑会出现在这里，下次不用再输设备 ID。</p>
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
  padding: 40px 20px;
  font-size: 13px;
  text-align: center;
}

@media (max-width: 560px) {
  .page {
    padding: 22px 16px 40px;
  }
}
</style>
