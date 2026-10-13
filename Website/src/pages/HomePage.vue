<script setup lang="ts">
import { computed, onMounted } from 'vue'
import AppWindowMock from '../components/AppWindowMock.vue'
import LineIcon, { type IconName } from '../components/LineIcon.vue'
import SiteFooter from '../components/SiteFooter.vue'
import SiteHeader from '../components/SiteHeader.vue'
import { loadReleases, megabytes, releases, type Platform } from '../releases'

onMounted(loadReleases)

// 有安装包时直接下载，还没问到时跳到下面的下载区
function downloadLink(platform: Platform): string {
  const release = releases[platform]
  return typeof release === 'object' ? release.url : '#download'
}

const modes: { icon: IconName; title: string; text: string }[] = [
  {
    icon: 'monitor',
    title: '我的设备',
    text: '同一个账号登录的电脑，点一下就连上：不用密码，不用有人点允许。锁屏了、停在登录界面也能连，还能让它重启、关机、睡眠、注销。',
  },
  {
    icon: 'people',
    title: '别人的电脑',
    text: '对方报给你设备 ID 和一串临时密码，再在他的电脑上点“允许”。临时密码只在两台电脑之间核对，服务器不知道；用过一次就换。',
  },
]

const features: { icon: IconName; title: string; text: string }[] = [
  {
    icon: 'bolt',
    title: '画面跟手',
    text: '显卡编码、显卡解码，H.264、HEVC、AV1 两边都支持的才用；画面到了就画，不排队。分辨率、帧率、码率都能自己挑。',
  },
  {
    icon: 'wifi',
    title: '能直连就直连',
    text: '同一网络里自动发现附近的设备，画面不绕外网；隔着网络也先试直连，打不通才经中继。从 Wi-Fi 换到热点，连接不断。',
  },
  {
    icon: 'displays',
    title: '多块显示器、声音',
    text: '对方有几块屏幕，工具栏上就有几个按钮，点哪块看哪块。对方电脑里放的声音，这边也听得到。',
  },
  {
    icon: 'file',
    title: '互传文件、共用剪贴板',
    text: '文件拖过去就发，断了下次接着传。这边复制、那边粘贴；密码管理器里标了“不要分享”的不会传。',
  },
  {
    icon: 'chat',
    title: '边连边聊',
    text: '连着的时候直接打字说话，不用另开聊天软件。记录只存在两台电脑上。',
  },
  {
    icon: 'power',
    title: '没人在也能连',
    text: '自己的电脑开机就能连，锁屏界面能解锁，Windows 上能按 Ctrl+Alt+Del。重启以后自己连回来。',
  },
]

const privacy = [
  { title: '画面、声音、键盘鼠标端到端加密。', text: '直连打不通走中继时，中继只看得到加密后的数据，也不保存。' },
  { title: '临时密码服务器不知道。', text: '它只在两台电脑之间核对。' },
  { title: '没有广告，没有第三方统计。', text: '对话、文件、连接记录只在你自己的电脑上。' },
]

const downloads: { platform: Platform; name: string; requirement: string; note: string; button: string }[] = [
  {
    platform: 'macos-arm64',
    name: 'macOS',
    requirement: 'macOS 14 及以上，Apple 芯片。',
    note: '装好后按引导授予“屏幕与系统音频录制”和“辅助功能”。',
    button: '下载 .pkg 安装包',
  },
  {
    platform: 'windows-x64',
    name: 'Windows',
    requirement: 'Windows 10 及以上，64 位。',
    note: '安装时需要管理员权限，用来装后台服务。',
    button: '下载安装程序',
  },
]

// 每个平台的下载卡片：问到了的安装包，或者还在问
const cards = computed(() =>
  downloads.map((item) => {
    const state = releases[item.platform]
    return { ...item, release: typeof state === 'object' ? state : null, state }
  }),
)
</script>

<template>
  <SiteHeader />
  <main>
    <section id="top" class="wrap hero">
      <div class="badge">远程控制 · macOS 与 Windows</div>
      <h1>天涯若比邻</h1>
      <p class="hero-text">在这台电脑前，用另一台。自己的电脑之间不用密码，帮别人时报一串临时密码就够了。</p>
      <div class="hero-buttons">
        <a :href="downloadLink('macos-arm64')" class="button primary">
          <LineIcon name="download" :size="18" :stroke="2" />
          下载 macOS 版
        </a>
        <a :href="downloadLink('windows-x64')" class="button">
          <LineIcon name="download" :size="18" :stroke="2" />
          下载 Windows 版
        </a>
      </div>
      <div class="requirements">macOS 14 及以上（Apple 芯片）· Windows 10 及以上（64 位）</div>
      <AppWindowMock class="mock" />
    </section>

    <section id="modes" class="wrap block">
      <div class="intro">
        <h2 class="section-title">两种连法，各管各的</h2>
        <p class="lead">比邻按“谁在连”定规矩：自己的电脑随时能连，别人的电脑要对方点头。</p>
      </div>
      <div class="modes">
        <div v-for="mode in modes" :key="mode.title" class="mode">
          <LineIcon :name="mode.icon" :size="28" />
          <h3>{{ mode.title }}</h3>
          <p>{{ mode.text }}</p>
        </div>
      </div>
    </section>

    <section id="features" class="wrap block">
      <h2 class="section-title">该有的都在</h2>
      <div class="features">
        <div v-for="feature in features" :key="feature.title" class="feature">
          <LineIcon :name="feature.icon" />
          <h3>{{ feature.title }}</h3>
          <p>{{ feature.text }}</p>
        </div>
      </div>
    </section>

    <section id="privacy" class="wrap block">
      <div class="privacy">
        <div class="privacy-quote">
          <div class="privacy-label">隐私</div>
          <p class="quote">能不收的就不收，能只存在你电脑上的就不上传。</p>
          <a href="./privacy.html" class="privacy-link">读隐私政策全文</a>
        </div>
        <div class="privacy-points">
          <div v-for="point in privacy" :key="point.title"><strong>{{ point.title }}</strong>{{ point.text }}</div>
        </div>
      </div>
    </section>

    <section id="download" class="wrap block">
      <div class="intro centered">
        <h2 class="section-title">下载比邻</h2>
        <p class="lead">两台电脑都装上，一台报 ID，另一台连。</p>
      </div>
      <div class="downloads">
        <div v-for="card in cards" :key="card.platform" class="download">
          <h3>{{ card.name }}</h3>
          <p>{{ card.requirement }}<br>{{ card.note }}</p>
          <template v-if="card.release">
            <a :href="card.release.url" class="button primary" download>{{ card.button }}</a>
            <span class="version">版本 {{ card.release.version }} · {{ megabytes(card.release) }}</span>
          </template>
          <span v-else-if="card.state === 'loading'" class="version">正在查最新版本…</span>
          <span v-else-if="card.state === 'unpublished'" class="version">还没有发布，敬请期待</span>
          <span v-else class="version">暂时取不到安装包，过一会儿再试</span>
        </div>
      </div>
      <p class="update-note">装好以后，程序里就能检查更新。</p>
    </section>
  </main>
  <SiteFooter />
</template>

<style scoped>
.hero {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 18px;
  padding-top: 64px;
  text-align: center;
}

.badge {
  padding: 5px 12px;
  border-radius: 999px;
  background: var(--fill);
  font-size: 13px;
  font-weight: 500;
  color: var(--text-secondary);
}

h1 {
  font-size: clamp(40px, 6.4vw, 76px);
  font-weight: 900;
  letter-spacing: 0.04em;
  line-height: 1.05;
}

.hero-text {
  max-width: 560px;
  font-size: clamp(15px, 1.7vw, 17px);
  line-height: 1.7;
  color: var(--text-secondary);
}

.hero-buttons {
  display: flex;
  flex-wrap: wrap;
  justify-content: center;
  gap: 12px;
  margin-top: 8px;
}

.requirements {
  font-size: 13px;
  color: var(--text-muted);
}

.mock {
  margin-top: 40px;
}

.block {
  display: flex;
  flex-direction: column;
  gap: 32px;
  padding-top: 104px;
}

.intro {
  display: flex;
  flex-direction: column;
  gap: 12px;
  max-width: 640px;
}

.intro.centered {
  align-self: center;
  align-items: center;
  text-align: center;
}

.modes,
.downloads {
  display: flex;
  flex-wrap: wrap;
  gap: 20px;
}

.mode,
.download {
  flex: 1 1 340px;
  display: flex;
  flex-direction: column;
  gap: 12px;
  padding: 28px;
  border-radius: 16px;
  background: var(--card);
}

.mode h3 {
  font-size: 20px;
  font-weight: 700;
}

.mode p {
  font-size: 14px;
  line-height: 1.8;
  color: var(--text-secondary);
}

.features {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
  gap: 32px 28px;
}

.feature {
  display: flex;
  flex-direction: column;
  gap: 10px;
}

.feature h3 {
  font-size: 16px;
  font-weight: 700;
}

.feature p {
  font-size: 14px;
  line-height: 1.75;
  color: var(--text-secondary);
}

.privacy {
  display: flex;
  flex-wrap: wrap;
  gap: 40px;
  padding: clamp(28px, 5vw, 64px);
  border-radius: 22px;
  background: var(--panel);
  color: #ffffff;
}

.privacy-quote {
  flex: 1 1 420px;
  display: flex;
  flex-direction: column;
  align-items: flex-start;
  gap: 18px;
}

.privacy-label {
  font-size: 13px;
  color: var(--panel-muted);
}

.quote {
  font-size: clamp(22px, 2.8vw, 31px);
  font-weight: 700;
  line-height: 1.45;
}

.privacy-link {
  font-size: 14px;
  text-decoration: underline;
  text-underline-offset: 4px;
}

.privacy-points {
  flex: 1 1 360px;
  display: flex;
  flex-direction: column;
  gap: 18px;
  font-size: 14px;
  line-height: 1.75;
  color: var(--panel-text);
}

.privacy-points strong {
  color: #ffffff;
}

.download {
  gap: 14px;
}

.download h3 {
  font-size: 22px;
  font-weight: 700;
}

.download p {
  font-size: 14px;
  line-height: 1.7;
  color: var(--text-secondary);
}

.download .button {
  align-self: flex-start;
  margin-top: 8px;
  font-size: 14px;
  padding: 11px 20px;
}

.version {
  font-size: 13px;
  color: var(--text-muted);
}

.update-note {
  text-align: center;
  font-size: 13px;
  color: var(--text-muted);
}
</style>
