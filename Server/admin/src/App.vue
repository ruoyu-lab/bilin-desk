<script setup lang="ts">
import { onMounted, onUnmounted, ref } from 'vue'
import { api, signedIn } from './api'
import LineIcon from './components/LineIcon.vue'
import type { IconName } from './icons'
import LoginView from './components/LoginView.vue'
import LogoMark from '@shared/LogoMark.vue'
import BackupsPage from './pages/BackupsPage.vue'
import FeedbackPage from './pages/FeedbackPage.vue'
import LogsPage from './pages/LogsPage.vue'
import OverviewPage from './pages/OverviewPage.vue'
import ReleasesPage from './pages/ReleasesPage.vue'
import { toasts } from './toasts'

// 管理后台（docs/admin.md）：左边导航栏，右边一页（和官网一个样子）；页面跟着地址的 #（刷新以后还在同一页）
type Page = 'overview' | 'feedback' | 'releases' | 'backups' | 'logs'
const pages: Array<{ page: Page; title: string; icon: IconName }> = [
  { page: 'overview', title: '概览', icon: 'chart' },
  { page: 'feedback', title: '用户反馈', icon: 'message' },
  { page: 'releases', title: '版本发布', icon: 'package' },
  { page: 'backups', title: '数据备份', icon: 'database' },
  { page: 'logs', title: '出错日志', icon: 'alert' },
]

function pageFromHash(): Page {
  const name = location.hash.slice(1)
  return pages.some((p) => p.page === name) ? (name as Page) : 'overview'
}

const page = ref<Page>(pageFromHash())
// 导航栏下面写着是哪台服务器
const host = location.host
const onHashChange = () => (page.value = pageFromHash())

onMounted(async () => {
  window.addEventListener('hashchange', onHashChange)
  // 还登录着（cookie 还有效）就直接进去
  try {
    await api('GET', 'status')
    signedIn.value = true
  } catch {
    signedIn.value = false
  }
})
onUnmounted(() => window.removeEventListener('hashchange', onHashChange))

async function logout() {
  await api('POST', 'logout')
  signedIn.value = false
}
</script>

<template>
  <LoginView v-if="signedIn === false" @signed-in="signedIn = true" />
  <div v-else-if="signedIn" class="shell">
    <aside class="sidebar">
      <a href="#overview" class="brand">
        <LogoMark :size="26" />
        <span>
          <strong>比邻</strong>
          <small>管理后台</small>
        </span>
      </a>
      <nav class="nav">
        <a v-for="p in pages" :key="p.page" :href="`#${p.page}`" :class="{ current: page === p.page }">
          <LineIcon :name="p.icon" :size="17" />{{ p.title }}
        </a>
      </nav>
      <div class="foot">
        <span class="host">{{ host }}</span>
        <a href="#" @click.prevent="logout"><LineIcon name="log-out" :size="17" />退出</a>
      </div>
    </aside>
    <main class="content">
      <OverviewPage v-if="page === 'overview'" />
      <FeedbackPage v-else-if="page === 'feedback'" />
      <ReleasesPage v-else-if="page === 'releases'" />
      <BackupsPage v-else-if="page === 'backups'" />
      <LogsPage v-else />
    </main>
  </div>
  <div class="toasts">
    <div v-for="t in toasts" :key="t.id" class="toast" :class="{ bad: t.bad }">{{ t.text }}</div>
  </div>
</template>

<style scoped>
.shell {
  display: grid;
  grid-template-columns: 216px minmax(0, 1fr);
  min-height: 100vh;
}

.sidebar {
  position: sticky;
  top: 0;
  display: flex;
  flex-direction: column;
  gap: 4px;
  height: 100vh;
  padding: 20px 14px;
  background: var(--sidebar);
}

.brand {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 0 8px 22px;
}

.brand strong {
  display: block;
  font-size: 15px;
  font-weight: 700;
}

.brand small {
  display: block;
  font-size: 12px;
  color: var(--text-muted);
}

.nav {
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.nav a,
.foot a {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 9px 12px;
  border-radius: 8px;
  font-size: 14px;
  color: var(--text-secondary);
  transition: background 0.15s, color 0.15s;
}

.nav a:hover,
.foot a:hover {
  background: var(--fill);
  color: var(--text);
}

.nav a.current {
  background: var(--selected);
  color: var(--text);
  font-weight: 500;
}

.foot {
  display: flex;
  flex-direction: column;
  gap: 2px;
  margin-top: auto;
}

.host {
  padding: 0 12px 8px;
  font-size: 12px;
  color: var(--text-muted);
  overflow-wrap: anywhere;
}

.content {
  width: 100%;
  max-width: var(--content);
  margin: 0 auto;
  padding: 36px 40px 80px;
}

.toasts {
  position: fixed;
  left: 50%;
  bottom: 24px;
  z-index: 10;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 8px;
  transform: translateX(-50%);
}

.toast {
  padding: 10px 18px;
  border-radius: 999px;
  background: var(--text);
  color: #ffffff;
  font-size: 13px;
  white-space: nowrap;
  box-shadow: 0 10px 30px rgba(22, 23, 26, 0.18);
  animation: rise 0.2s ease;
}

.toast.bad {
  background: var(--danger);
}

@keyframes rise {
  from {
    opacity: 0;
    transform: translateY(6px);
  }
}

/* 窄屏：导航栏挪到上面一行 */
@media (max-width: 760px) {
  .shell {
    grid-template-columns: 1fr;
  }
  .sidebar {
    z-index: 5;
    flex-direction: row;
    align-items: center;
    height: auto;
    padding: 10px 12px;
    overflow-x: auto;
  }
  .brand {
    padding: 0 8px 0 0;
  }
  .brand span {
    display: none;
  }
  .nav {
    flex-direction: row;
  }
  .nav a,
  .foot a {
    flex-shrink: 0;
  }
  .foot {
    margin: 0 0 0 auto;
  }
  .host {
    display: none;
  }
  .content {
    padding: 24px 16px 64px;
  }
}
</style>
