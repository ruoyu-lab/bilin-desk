<script setup lang="ts">
import LineIcon from './LineIcon.vue'
import type { IconName } from '../icons'

// 首屏的程序主窗口（概览页）：照着程序画的（App/qml/main/OverviewPage.qml、Sidebar.qml，颜色是 Theme.qml 的），
// 网页是深色时画程序的深色。只是画出来的，不能点
const navigation: Array<{ title: string; icon: IconName }> = [
  { title: '概览', icon: 'home' },
  { title: '我的设备', icon: 'monitor' },
  { title: '附近', icon: 'radar' },
  { title: '收藏', icon: 'star' },
  { title: '最近', icon: 'clock' },
]
const mine = [
  { name: '书房的 PC', system: 'windows', online: true, detail: '416 902 934' },
  { name: '办公室 Mac mini', system: 'apple', online: true, detail: '731 204 586' },
  { name: '爸妈家的笔记本', system: 'windows', online: false, detail: '208 553 617' },
] as const
const nearby = [{ name: '客厅电脑', system: 'windows', online: true, detail: '192.168.1.23' }] as const
</script>

<template>
  <div class="window" aria-hidden="true">
    <aside class="sidebar">
      <div class="lights">
        <span style="background: #ff5f57" />
        <span style="background: #febc2e" />
        <span style="background: #28c840" />
      </div>
      <div v-for="(item, index) in navigation" :key="item.title" class="nav-item" :class="{ current: index === 0 }">
        <LineIcon :name="item.icon" :size="16" :stroke="2" />
        {{ item.title }}
      </div>
      <div class="account">
        <span class="avatar">禾</span>
        <span class="account-name">阿禾</span>
        <LineIcon name="settings" :size="16" :stroke="2" />
      </div>
    </aside>

    <div class="main">
      <div class="toolbar">
        <LineIcon name="search" :size="16" :stroke="2" />
        <LineIcon name="plus" :size="16" :stroke="2" />
        <LineIcon name="bell" :size="16" :stroke="2" />
      </div>
      <div class="content">
        <div class="cards">
          <div class="card id-card">
            <div class="label">本机设备 ID</div>
            <div class="row">
              <span class="id">128 406 395</span>
              <LineIcon name="copy" :size="16" :stroke="2" />
            </div>
            <div class="row password">
              <span>临时密码 <b>k7m4qx</b></span>
              <span class="icons">
                <LineIcon name="refresh" :size="15" :stroke="2" />
                <LineIcon name="copy" :size="15" :stroke="2" />
              </span>
            </div>
          </div>
          <div class="card connect-card">
            <div class="title">连接到设备</div>
            <div class="connect-row">
              <span class="input">设备 ID、IP 或名称</span>
              <span class="connect-button">连接</span>
            </div>
            <div class="segmented">
              <span class="on">控制</span>
              <span>仅查看</span>
            </div>
          </div>
        </div>

        <div class="section">我的设备 <small>同一个账号登录的设备</small></div>
        <div class="tiles">
          <div v-for="device in mine" :key="device.name" class="tile">
            <span class="os" :class="device.system"><LineIcon :name="device.system" :size="17" /></span>
            <span class="tile-text">
              <span class="tile-name">{{ device.name }}</span>
              <span class="tile-state">
                <span class="dot" :class="{ online: device.online }" />
                {{ device.online ? '在线' : '离线' }} · {{ device.detail }}
              </span>
            </span>
          </div>
        </div>

        <div class="section">附近</div>
        <div class="tiles">
          <div v-for="device in nearby" :key="device.name" class="tile">
            <span class="os" :class="device.system"><LineIcon :name="device.system" :size="17" /></span>
            <span class="tile-text">
              <span class="tile-name">{{ device.name }}</span>
              <span class="tile-state"><span class="dot online" />在线 · {{ device.detail }}</span>
            </span>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
/* 程序浅色时的颜色（App/qml/Theme.qml） */
.window {
  --m-bg: #ffffff;
  --m-sidebar: #f7f7f9;
  --m-card: #f6f7fa;
  --m-surface: #ffffff;
  --m-selected: rgba(0, 0, 0, 0.08);
  --m-text: #1d1d1f;
  --m-text-2: #6e6e73;
  --m-text-3: #aeaeb2;
  --m-success: #34c759;
  --m-avatar: #e3e6ec;
  --m-avatar-text: #3a3a3c;
  --m-win: #e5f0ff;
  --m-win-ink: #0a6fe0;
  --m-mac: #eceef2;
  --m-mac-ink: #3a3a3c;
  --m-accent: #007aff;
  --m-segment: #e9e9ed;

  width: 100%;
  max-width: 960px;
  display: flex;
  text-align: left;
  color: var(--m-text);
  background: var(--m-bg);
  border-radius: 14px;
  overflow: hidden;
  box-shadow: 0 30px 80px rgba(22, 23, 26, 0.14), 0 2px 8px rgba(22, 23, 26, 0.06);
}

/* 程序深色时的颜色 */
@media (prefers-color-scheme: dark) {
  .window {
    --m-bg: #1e1e20;
    --m-sidebar: #252527;
    --m-card: #29292b;
    --m-surface: #2c2c2e;
    --m-selected: rgba(255, 255, 255, 0.13);
    --m-text: #f2f2f4;
    --m-text-2: #a1a1a6;
    --m-text-3: #6c6c70;
    --m-success: #30d158;
    --m-avatar: #3a3a3d;
    --m-avatar-text: #e5e5ea;
    --m-win: #1b2c45;
    --m-win-ink: #5aa2ff;
    --m-mac: #323236;
    --m-mac-ink: #d1d1d6;
    --m-accent: #0a84ff;
    --m-segment: #3a3a3d;

    box-shadow: 0 30px 80px rgba(0, 0, 0, 0.5), 0 0 0 1px rgba(255, 255, 255, 0.06);
  }
}

.sidebar {
  width: 196px;
  flex-shrink: 0;
  display: flex;
  flex-direction: column;
  gap: 2px;
  padding: 16px 10px 12px;
  background: var(--m-sidebar);
}

.lights {
  display: flex;
  gap: 8px;
  padding: 0 6px 20px;
}

.lights span {
  width: 12px;
  height: 12px;
  border-radius: 50%;
}

.nav-item {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 8px 10px;
  border-radius: 8px;
  font-size: 13px;
  color: var(--m-text);
}

.nav-item.current {
  background: var(--m-selected);
}

.account {
  margin-top: auto;
  display: flex;
  align-items: center;
  gap: 9px;
  padding: 8px 8px;
  font-size: 13px;
  color: var(--m-text-2);
}

.account-name {
  flex: 1;
  color: var(--m-text);
}

.avatar {
  width: 26px;
  height: 26px;
  border-radius: 50%;
  display: flex;
  align-items: center;
  justify-content: center;
  background: var(--m-avatar);
  color: var(--m-avatar-text);
  font-size: 12px;
}

.main {
  flex: 1 1 auto;
  min-width: 0;
  display: flex;
  flex-direction: column;
}

.toolbar {
  display: flex;
  justify-content: flex-end;
  gap: 18px;
  padding: 16px 22px 6px;
  color: var(--m-text-2);
}

.content {
  display: flex;
  flex-direction: column;
  gap: 14px;
  padding: 6px 24px 26px;
}

.cards {
  display: flex;
  flex-wrap: wrap;
  gap: 14px;
}

.card {
  display: flex;
  flex-direction: column;
  gap: 8px;
  padding: 16px 18px;
  border-radius: 12px;
  background: var(--m-card);
}

.id-card {
  flex: 10 1 240px;
}

.connect-card {
  flex: 13 1 300px;
}

.label {
  font-size: 12px;
  color: var(--m-text-2);
}

.title {
  font-size: 14px;
  font-weight: 600;
}

.row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  color: var(--m-text-2);
}

.id {
  font-size: 26px;
  font-weight: 600;
  letter-spacing: 0.5px;
  color: var(--m-text);
}

.password {
  margin-top: 6px;
  font-size: 12px;
}

.password b {
  margin-left: 6px;
  font-size: 14px;
  font-weight: 600;
  letter-spacing: 1px;
  color: var(--m-text);
}

.icons {
  display: flex;
  gap: 12px;
}

.connect-row {
  display: flex;
  gap: 8px;
}

.input {
  flex: 1;
  padding: 9px 12px;
  border-radius: 8px;
  background: var(--m-surface);
  font-size: 13px;
  color: var(--m-text-3);
}

.connect-button {
  padding: 9px 16px;
  border-radius: 8px;
  background: var(--m-accent);
  color: #ffffff;
  font-size: 13px;
}

.segmented {
  display: flex;
  align-self: flex-start;
  padding: 2px;
  border-radius: 8px;
  background: var(--m-segment);
  font-size: 12px;
}

.segmented span {
  padding: 4px 14px;
  border-radius: 6px;
  color: var(--m-text-2);
}

.segmented .on {
  background: var(--m-surface);
  color: var(--m-text);
}

.section {
  margin-top: 8px;
  font-size: 15px;
  font-weight: 600;
}

.section small {
  margin-left: 6px;
  font-size: 12px;
  font-weight: 400;
  color: var(--m-text-3);
}

.tiles {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));
  gap: 10px;
}

.tile {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 12px 14px;
  border-radius: 10px;
  background: var(--m-card);
}

.os {
  width: 36px;
  height: 36px;
  flex-shrink: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  border-radius: 9px;
}

.os.windows {
  background: var(--m-win);
  color: var(--m-win-ink);
}

.os.apple {
  background: var(--m-mac);
  color: var(--m-mac-ink);
}

.tile-text {
  min-width: 0;
  display: flex;
  flex-direction: column;
  gap: 3px;
}

.tile-name {
  overflow: hidden;
  font-size: 13px;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.tile-state {
  display: flex;
  align-items: center;
  gap: 5px;
  font-size: 12px;
  color: var(--m-text-2);
}

.dot {
  width: 6px;
  height: 6px;
  border-radius: 50%;
  background: var(--m-text-3);
}

.dot.online {
  background: var(--m-success);
}

/* 手机上放不下侧栏 */
@media (max-width: 760px) {
  .sidebar {
    display: none;
  }

  .content {
    padding: 6px 16px 18px;
  }
}
</style>
