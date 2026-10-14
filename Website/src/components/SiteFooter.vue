<script setup lang="ts">
import { SOURCE_URL } from '../links'
</script>

<template>
  <footer class="wrap footer">
    <span class="name">比邻</span>
    <span>免费开源（AGPL-3.0）· 无广告</span>
    <a :href="SOURCE_URL">源代码</a>
    <a href="./terms.html">服务协议</a>
    <a href="./privacy.html">隐私政策</a>
    <span class="copyright">© 2026 比邻</span>
  </footer>
</template>

<style scoped>
.footer {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 20px 32px;
  padding-top: 96px;
  padding-bottom: 40px;
  font-size: 13px;
  color: var(--text-secondary);
}

.footer a:hover {
  color: var(--text);
}

.name {
  font-weight: 700;
  color: var(--text);
}

.copyright {
  margin-left: auto;
  color: var(--text-muted);
}
</style>
