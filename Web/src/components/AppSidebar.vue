<script setup lang="ts">
import { go, openLogin, route, state, type Page } from '../state'
import LineIcon from './LineIcon.vue'
import type { IconName } from '../icons'
import LogoMark from '@shared/LogoMark.vue'

// 左边的导航栏（照着程序的 Sidebar.qml）：上面几页，最下面是账号和设置
const pages: Array<{ page: Page; path: string; title: string; icon: IconName }> = [
  { page: 'home', path: '', title: '概览', icon: 'home' },
  { page: 'devices', path: 'devices', title: '我的设备', icon: 'monitor' },
  { page: 'recent', path: 'recent', title: '最近', icon: 'clock' },
]
</script>

<template>
  <aside class="sidebar">
    <a href="#/" class="brand">
      <LogoMark :size="26" />
      <span><strong>比邻</strong><small>网页版</small></span>
    </a>
    <nav class="nav">
      <a v-for="p in pages" :key="p.page" :href="`#/${p.path}`" :class="{ current: route.page === p.page }">
        <LineIcon :name="p.icon" :size="17" />{{ p.title }}
      </a>
    </nav>
    <div class="foot">
      <button v-if="state.account" class="account" type="button" @click="go('settings')">
        <span class="avatar">{{ state.account.nickname.slice(0, 1) }}</span>
        <span class="name">{{ state.account.nickname }}</span>
      </button>
      <button v-else class="account" type="button" @click="openLogin">
        <span class="avatar"><LineIcon name="user" :size="15" /></span>
        <span class="name">登录</span>
      </button>
      <a href="#/settings" class="icon-button" :class="{ on: route.page === 'settings' }" aria-label="设置">
        <LineIcon name="settings" :size="17" />
      </a>
    </div>
  </aside>
</template>

<style scoped>
.sidebar {
  display: flex;
  flex-direction: column;
  gap: 4px;
  width: 216px;
  flex-shrink: 0;
  height: 100%;
  padding: 18px 12px 14px;
  background: var(--sidebar);
}

.brand {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 0 8px 20px;
}

.brand strong {
  display: block;
  font-size: 15px;
  font-weight: 700;
}

.brand small {
  display: block;
  font-size: 12px;
  color: var(--text-secondary);
}

.nav {
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.nav a {
  display: flex;
  align-items: center;
  gap: 10px;
  height: 36px;
  padding: 0 12px;
  border-radius: var(--radius);
  font-size: 14px;
  color: var(--text);
  transition: background 0.15s;
}

.nav a:hover {
  background: var(--hover);
}

.nav a.current {
  background: var(--selected);
  font-weight: 500;
}

.nav a svg {
  color: var(--text-secondary);
}

.foot {
  display: flex;
  align-items: center;
  gap: 6px;
  margin-top: auto;
}

.account {
  display: flex;
  flex: 1;
  align-items: center;
  gap: 10px;
  min-width: 0;
  height: 40px;
  padding: 0 6px;
  border: 0;
  border-radius: var(--radius);
  background: transparent;
  text-align: left;
  cursor: pointer;
}

.account:hover {
  background: var(--hover);
}

.avatar {
  display: grid;
  flex-shrink: 0;
  place-items: center;
  width: 28px;
  height: 28px;
  border-radius: 50%;
  background: var(--avatar);
  color: var(--avatar-text);
  font-size: 12px;
}

.name {
  overflow: hidden;
  font-size: 14px;
  text-overflow: ellipsis;
  white-space: nowrap;
}

@media (max-width: 760px) {
  .sidebar {
    width: 64px;
    padding: 18px 10px 14px;
  }
  .brand span,
  .name {
    display: none;
  }
  .brand {
    padding: 0 9px 20px;
  }
  .nav a {
    justify-content: center;
    padding: 0;
    font-size: 0;
    gap: 0;
  }
  .foot {
    flex-direction: column;
  }
  .account {
    justify-content: center;
    flex: none;
  }
}
</style>
