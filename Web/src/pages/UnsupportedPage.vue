<script setup lang="ts">
import LogoMark from '@shared/LogoMark.vue'
import LineIcon from '../components/LineIcon.vue'

// 不是 Chrome、Edge 时：网页版先只在这两个浏览器里能用（SDK 的浏览器包只在 Chromium 上验证过）
</script>

<template>
  <div class="unsupported">
    <LogoMark :size="56" />
    <h1>请用 Chrome 或 Edge 打开</h1>
    <p class="muted">比邻网页版现在只支持 Chrome 和 Edge 浏览器。也可以下载客户端，画面更清晰流畅。</p>
    <div class="actions">
      <a class="button dark large" href="https://www.bilindesk.com/#download">
        <LineIcon name="download" :size="16" />下载客户端
      </a>
      <a class="button large" href="https://www.google.cn/chrome/" target="_blank" rel="noopener">下载 Chrome</a>
    </div>
  </div>
</template>

<style scoped>
.unsupported {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 12px;
  min-height: 100%;
  padding: 40px 24px;
  text-align: center;
}

h1 {
  margin-top: 14px;
  font-size: 22px;
  font-weight: 600;
}

p {
  max-width: 380px;
  font-size: 14px;
  line-height: 1.7;
}

.actions {
  display: flex;
  flex-wrap: wrap;
  justify-content: center;
  gap: 10px;
  margin-top: 14px;
}
</style>
