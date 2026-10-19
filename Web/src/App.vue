<script setup lang="ts">
import AppSidebar from './components/AppSidebar.vue'
import LoginDialog from './components/LoginDialog.vue'
import PasswordDialog from './components/PasswordDialog.vue'
import DevicesPage from './pages/DevicesPage.vue'
import FavoritesPage from './pages/FavoritesPage.vue'
import HomePage from './pages/HomePage.vue'
import RecentPage from './pages/RecentPage.vue'
import SessionPage from './pages/SessionPage.vue'
import SettingsPage from './pages/SettingsPage.vue'
import UnsupportedPage from './pages/UnsupportedPage.vue'
import { askingPassword, closeLogin, closePassword, loginOpen, openSession, route, supported } from './state'

// 网页版（docs/web.md）：左边导航栏，右边一页；会话在新的标签页里，整个页面是它。页面跟着地址的 #
const connectWithPassword = (password: string) => {
  const device = askingPassword.value
  closePassword()
  if (device) openSession(device.id, password)
}
</script>

<template>
  <UnsupportedPage v-if="!supported || route.page === 'unsupported'" />
  <SessionPage v-else-if="route.page === 'session'" :id="route.id" :key="route.id" :handoff="route.handoff" />
  <div v-else class="shell">
    <AppSidebar />
    <main class="content">
      <HomePage v-if="route.page === 'home'" />
      <DevicesPage v-else-if="route.page === 'devices'" />
      <FavoritesPage v-else-if="route.page === 'favorites'" />
      <RecentPage v-else-if="route.page === 'recent'" />
      <SettingsPage v-else />
    </main>
  </div>
  <LoginDialog v-if="loginOpen" @close="closeLogin" />
  <PasswordDialog v-if="askingPassword" :device="askingPassword" @close="closePassword"
                  @connect="connectWithPassword" />
</template>

<style scoped>
.shell {
  display: flex;
  height: 100%;
}

.content {
  flex: 1;
  min-width: 0;
  overflow-y: auto;
}
</style>
