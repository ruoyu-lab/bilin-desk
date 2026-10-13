<script setup lang="ts">
import { SOURCE_URL } from '../links'
import LogoMark from './LogoMark.vue'

// GitHub 的标志（16×16）
const github = 'M8 0C3.58 0 0 3.58 0 8c0 3.54 2.29 6.53 5.47 7.59.4.07.55-.17.55-.38 0-.19-.01-.82-.01-1.49-2.01.37-2.53-.49-2.69-.94-.09-.23-.48-.94-.82-1.13-.28-.15-.68-.52-.01-.53.63-.01 1.08.58 1.23.82.72 1.21 1.87.87 2.33.66.07-.52.28-.87.51-1.07-1.78-.2-3.64-.89-3.64-3.95 0-.87.31-1.59.82-2.15-.08-.2-.36-1.02.08-2.12 0 0 .67-.21 2.2.82.64-.18 1.32-.27 2-.27.68 0 1.36.09 2 .27 1.53-1.04 2.2-.82 2.2-.82.44 1.1.16 1.92.08 2.12.51.56.82 1.27.82 2.15 0 3.07-1.87 3.75-3.65 3.95.29.25.54.73.54 1.48 0 1.07-.01 1.93-.01 2.2 0 .21.15.46.55.38A8.013 8.013 0 0 0 16 8c0-4.42-3.58-8-8-8z'
</script>

<template>
  <header class="header">
    <nav class="wrap nav">
      <a href="./" class="brand">
        <LogoMark :size="28" />
        比邻
      </a>
      <div class="links">
        <a href="./#modes">怎么连</a>
        <a href="./#features">功能</a>
        <a href="./#privacy">隐私</a>
        <a href="./#download">下载</a>
      </div>
      <a :href="SOURCE_URL" class="button small source" aria-label="GitHub 上的源代码">
        <svg width="16" height="16" viewBox="0 0 16 16" fill="currentColor" aria-hidden="true"><path :d="github" /></svg>
        GitHub
      </a>
      <a href="./#download" class="button primary small">免费下载</a>
    </nav>
  </header>
</template>

<style scoped>
.header {
  position: sticky;
  top: 0;
  z-index: 5;
  background: var(--header);
  backdrop-filter: blur(12px);
  -webkit-backdrop-filter: blur(12px);
}

.nav {
  display: flex;
  align-items: center;
  gap: 28px;
  padding-top: 14px;
  padding-bottom: 14px;
}

.brand {
  display: flex;
  align-items: center;
  gap: 10px;
  font-size: 17px;
  font-weight: 700;
}

.links {
  display: flex;
  gap: 24px;
  font-size: 14px;
  color: var(--text-secondary);
}

.links a:hover {
  color: var(--text);
}

/* GitHub 在"免费下载"左边，两个一起靠右 */
.source {
  margin-left: auto;
  margin-right: -16px;
}

@media (max-width: 760px) {
  .links {
    display: none;
  }
}
</style>
