<script setup lang="ts">
import { computed } from 'vue'
import DeviceList from '../components/DeviceList.vue'
import { favoriteDevices } from '../state'

// 收藏：常连的电脑（列表里点星标收藏），在线的排在前面。记在这个浏览器里
const devices = computed(() => [...favoriteDevices()].sort((a, b) => Number(b.online) - Number(a.online)))
</script>

<template>
  <div class="page">
    <h1>收藏</h1>
    <p class="intro muted">常连的电脑。在我的设备、最近里点星标就收藏到这里。</p>
    <DeviceList v-if="devices.length > 0" :devices="devices" />
    <div v-else class="card empty">
      <p class="muted">还没有收藏的电脑。</p>
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
