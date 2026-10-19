<script setup lang="ts">
import LineIcon from '../components/LineIcon.vue'
import { openLogin, scheme, setScheme, signOut, state, type Scheme } from '../state'

// 设置：账号、外观、关于网页版（照着程序的 SettingsPage.qml：一组一张卡片，一行一项）
const schemes: Array<{ value: Scheme; title: string }> = [
  { value: 'system', title: '跟随系统' },
  { value: 'light', title: '浅色' },
  { value: 'dark', title: '深色' },
]
</script>

<template>
  <div class="page">
    <h1>设置</h1>

    <div class="card account">
      <template v-if="state.account">
        <span class="avatar large">{{ state.account.nickname.slice(0, 1) }}</span>
        <div class="who">
          <div class="title">{{ state.account.nickname }}</div>
          <div class="muted small">{{ state.account.email }}</div>
        </div>
        <button class="button" type="button" @click="signOut">
          <LineIcon name="log-out" :size="15" />退出登录
        </button>
      </template>
      <template v-else>
        <span class="avatar large"><LineIcon name="user" :size="18" /></span>
        <div class="who">
          <div class="title">没有登录</div>
          <div class="muted small">登录以后，同一个账号的电脑不用密码就能连</div>
        </div>
        <button class="button dark" type="button" @click="openLogin">登录 / 注册</button>
      </template>
    </div>

    <h2>通用</h2>
    <div class="card rows">
      <div class="row">
        <span class="tile-icon"><LineIcon name="palette" :size="14" /></span>
        <span class="label">外观</span>
        <div class="segmented">
          <button v-for="s in schemes" :key="s.value" type="button" :class="{ on: scheme === s.value }"
                  @click="setScheme(s.value)">{{ s.title }}</button>
        </div>
      </div>
    </div>

    <h2>关于网页版</h2>
    <div class="card rows">
      <div class="row">
        <span class="tile-icon"><LineIcon name="info" :size="14" /></span>
        <div class="label">
          画质
          <div class="detail">网页版最高 1080p、每秒 30 帧，画面经比邻的服务器转发</div>
        </div>
      </div>
      <a class="row link" href="https://www.bilindesk.com/#download" target="_blank" rel="noopener">
        <span class="tile-icon"><LineIcon name="download" :size="14" /></span>
        <div class="label">
          下载客户端
          <div class="detail">更清晰流畅（4K、每秒 60 帧以上），能直连，也能让别人连这台电脑</div>
        </div>
        <LineIcon name="chevron-right" :size="14" class="chevron" />
      </a>
      <a class="row link" href="https://www.bilindesk.com/terms.html" target="_blank" rel="noopener">
        <span class="label">服务协议</span>
        <LineIcon name="chevron-right" :size="14" class="chevron" />
      </a>
      <a class="row link" href="https://www.bilindesk.com/privacy.html" target="_blank" rel="noopener">
        <span class="label">隐私政策</span>
        <LineIcon name="chevron-right" :size="14" class="chevron" />
      </a>
    </div>
  </div>
</template>

<style scoped>
.page {
  max-width: 688px;
  margin: 0 auto;
  padding: 28px 24px 48px;
}

h1 {
  margin-bottom: 20px;
  font-size: 22px;
  font-weight: 600;
}

h2 {
  margin: 22px 0 6px 6px;
  font-size: 12px;
  font-weight: 400;
  color: var(--text-secondary);
}

.account {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 14px;
}

.avatar.large {
  display: grid;
  flex-shrink: 0;
  place-items: center;
  width: 40px;
  height: 40px;
  border-radius: 50%;
  background: var(--avatar);
  color: var(--avatar-text);
  font-size: 15px;
}

.who {
  flex: 1;
  min-width: 0;
}

.title {
  font-size: 15px;
  font-weight: 600;
}

.small {
  margin-top: 2px;
  font-size: 12px;
}

.rows {
  padding: 4px;
}

.row {
  display: flex;
  align-items: center;
  gap: 12px;
  min-height: 44px;
  padding: 8px 10px;
  border-radius: var(--radius);
}

.row + .row {
  box-shadow: 0 -1px 0 var(--border);
}

.row.link:hover {
  background: var(--hover);
  box-shadow: none;
}

.row.link:hover + .row {
  box-shadow: none;
}

.tile-icon {
  display: grid;
  flex-shrink: 0;
  place-items: center;
  width: 24px;
  height: 24px;
  border-radius: 6px;
  background: var(--surface);
}

.label {
  flex: 1;
  min-width: 0;
  font-size: 14px;
}

.detail {
  margin-top: 2px;
  font-size: 12px;
  line-height: 1.5;
  color: var(--text-secondary);
}

.chevron {
  color: var(--text-muted);
}

.segmented {
  display: inline-flex;
  padding: 2px;
  border-radius: var(--radius);
  background: var(--fill);
}

.segmented button {
  height: 26px;
  padding: 0 12px;
  border: 0;
  border-radius: 6px;
  background: transparent;
  font-size: 12px;
  color: var(--text-secondary);
  cursor: pointer;
}

.segmented button.on {
  background: var(--surface);
  color: var(--text);
  box-shadow: 0 1px 2px rgba(0, 0, 0, 0.08);
}
</style>
