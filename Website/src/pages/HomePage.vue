<script setup lang="ts">
import { computed, onMounted } from 'vue'
import AppWindowMock from '../components/AppWindowMock.vue'
import LineIcon from '../components/LineIcon.vue'
import type { IconName } from '../icons'
import SiteFooter from '../components/SiteFooter.vue'
import SiteHeader from '../components/SiteHeader.vue'
import { faq } from '../faq'
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
    text: '登录同一账号的设备之间一键直连，无需密码，也无需对方确认。设备处于锁屏或登录界面时同样可以连接，并支持远程重启、关机、睡眠与注销。',
  },
  {
    icon: 'people',
    title: '远程协助',
    text: '对方提供设备 ID 与临时密码，并在本机确认授权后即可建立连接。临时密码仅在两台设备之间校验，服务器无从获知，每次使用后自动更新。',
  },
]

const features: { icon: IconName; title: string; text: string; tags: string[] }[] = [
  {
    icon: 'bolt',
    title: '低延迟，高画质',
    text: '全程硬件编解码，自动选用双方均支持硬件加速的格式，优先 HEVC。分辨率与帧率最高可达对方显示器的原生规格，另提供文字清晰模式与低延迟模式。',
    tags: ['HEVC', 'H.264', 'AV1', 'VP9'],
  },
  {
    icon: 'wifi',
    title: '智能连接',
    text: '局域网内自动发现设备并直接连接，画面不经公网；跨网络时优先 P2P 直连，无法打通时自动切换至中继。网络切换时连接不中断。',
    tags: ['局域网直连', 'P2P', '自动中继'],
  },
  {
    icon: 'displays',
    title: '多显示器与音频',
    text: '工具栏实时显示对方每块显示器的缩略图，点击即可切换；远端设备播放的声音同步传输到本地。',
    tags: ['多屏切换', '远端音频'],
  },
  {
    icon: 'file',
    title: '文件与剪贴板',
    text: '拖放即可发送文件，中断后支持断点续传；剪贴板双向同步，密码管理器中标记为敏感的内容不会被传输。',
    tags: ['断点续传', '剪贴板同步'],
  },
  {
    icon: 'shield',
    title: '端到端加密',
    text: '画面、声音、键鼠输入、剪贴板、文件与消息均在两台设备之间端到端加密。服务器只负责协助双方建立连接，经中继转发时也仅接触加密数据，且不做任何留存。',
    tags: ['全程加密', '中继不留存'],
  },
  {
    icon: 'power',
    title: '无人值守访问',
    text: '本人设备开机即可连接，可在锁屏界面解锁，Windows 上支持发送 Ctrl+Alt+Del；设备重启后自动恢复在线，也可由同一网络内的其他设备远程唤醒。',
    tags: ['锁屏解锁', '远程唤醒'],
  },
]

const privacy = [
  { title: '全程端到端加密。', text: '画面、声音、键鼠输入、文件与消息均在两台设备之间加密传输；经中继转发时，中继只接触加密数据，且不做任何留存。' },
  { title: '临时密码不经服务器。', text: '密码仅在两台设备之间完成校验。' },
  { title: '无广告，无第三方统计。', text: '消息、文件与连接记录均只保存在您自己的设备上。' },
]

const downloads: { platform: Platform; name: string; requirement: string; note: string; button: string }[] = [
  {
    platform: 'macos-arm64',
    name: 'macOS',
    requirement: 'macOS 14 及以上，Apple 芯片。',
    note: '安装完成后，请按引导授予“屏幕与系统音频录制”及“辅助功能”权限。',
    button: '下载 .pkg 安装包',
  },
  {
    platform: 'windows-x64',
    name: 'Windows',
    requirement: 'Windows 10 及以上，64 位。',
    note: '安装过程需要管理员权限，用于部署后台服务。',
    button: '下载安装程序',
  },
  {
    platform: 'android-arm64',
    name: 'Android',
    requirement: 'Android 10 及以上，64 位。',
    note: '用手机连接并控制电脑，手机本身不会被远程控制。首次安装需允许浏览器“安装未知应用”。',
    button: '下载 .apk 安装包',
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
      <div class="badge">开源远程桌面 · macOS、Windows 与 Android</div>
      <h1>天涯若比邻</h1>
      <p class="hero-text">安全、流畅的跨平台远程桌面，全程端到端加密。<br>本人设备免密直连，远程协助凭一次性临时密码授权。</p>
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
      <div class="requirements">
        支持 macOS 14 及以上（Apple 芯片）与 Windows 10 及以上（64 位）；手机上可用
        <a href="#download">Android 版</a>连接电脑
      </div>
      <AppWindowMock class="mock" />
    </section>

    <section id="modes" class="wrap block">
      <div class="intro">
        <h2 class="section-title">两种连接方式，授权各自独立</h2>
        <p class="lead">比邻按连接对象区分授权策略：本人设备随时可达，他人设备须经对方确认。</p>
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
      <div class="intro">
        <h2 class="section-title">核心能力</h2>
        <p class="lead">从画质、网络到协作，覆盖远程办公与技术支持的完整场景。</p>
      </div>
      <div class="features">
        <div v-for="feature in features" :key="feature.title" class="feature">
          <h3>
            <LineIcon :name="feature.icon" :size="22" />
            {{ feature.title }}
          </h3>
          <p>{{ feature.text }}</p>
          <ul class="tags">
            <li v-for="tag in feature.tags" :key="tag">{{ tag }}</li>
          </ul>
        </div>
      </div>
    </section>

    <section id="privacy" class="wrap block">
      <div class="privacy">
        <div class="privacy-quote">
          <div class="privacy-label">隐私与安全</div>
          <p class="quote">非必要不收集，能留在本地的数据绝不上传。</p>
          <a href="./privacy.html" class="privacy-link">阅读完整隐私政策</a>
        </div>
        <div class="privacy-points">
          <div v-for="point in privacy" :key="point.title"><strong>{{ point.title }}</strong>{{ point.text }}</div>
        </div>
      </div>
    </section>

    <section id="download" class="wrap block">
      <div class="intro centered">
        <h2 class="section-title">下载比邻</h2>
        <p class="lead">在两台设备上分别安装比邻，通过设备 ID 即可发起连接。</p>
      </div>
      <div class="downloads">
        <div v-for="card in cards" :key="card.platform" class="download">
          <h3>{{ card.name }}</h3>
          <p>{{ card.requirement }}<br>{{ card.note }}</p>
          <template v-if="card.release">
            <a :href="card.release.url" class="button primary" download>{{ card.button }}</a>
            <span class="version">版本 {{ card.release.version }} · {{ megabytes(card.release) }}</span>
          </template>
          <span v-else-if="card.state === 'loading'" class="version">正在获取最新版本…</span>
          <span v-else-if="card.state === 'unpublished'" class="version">即将发布，敬请期待</span>
          <span v-else class="version">暂时无法获取安装包，请稍后重试</span>
        </div>
      </div>
      <p class="update-note">安装后可在应用内检查并获取更新。</p>
    </section>

    <section id="faq" class="wrap block">
      <div class="intro">
        <h2 class="section-title">常见问题</h2>
      </div>
      <div class="faq">
        <div v-for="item in faq" :key="item.question" class="question">
          <h3>{{ item.question }}</h3>
          <p>{{ item.answer }}</p>
        </div>
      </div>
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

.requirements a {
  color: var(--text-secondary);
  text-decoration: underline;
  text-underline-offset: 3px;
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
  grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
  gap: 20px;
}

.feature {
  display: flex;
  flex-direction: column;
  gap: 12px;
  padding: 28px;
  border-radius: 16px;
  background: var(--card);
}

.feature h3 {
  display: flex;
  align-items: center;
  gap: 10px;
  font-size: 18px;
  font-weight: 700;
}

.feature h3 svg {
  flex: none;
}

.feature p {
  flex: 1;
  font-size: 14px;
  line-height: 1.8;
  color: var(--text-secondary);
}

.tags {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
  margin: 4px 0 0;
  padding: 0;
  list-style: none;
}

.tags li {
  padding: 3px 10px;
  border-radius: 999px;
  background: var(--fill);
  font-size: 12px;
  font-weight: 500;
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

.faq {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
  gap: 20px;
}

.question {
  display: flex;
  flex-direction: column;
  gap: 10px;
  padding: 24px 28px;
  border-radius: 16px;
  background: var(--card);
}

.question h3 {
  font-size: 16px;
  font-weight: 700;
}

.question p {
  font-size: 14px;
  line-height: 1.8;
  color: var(--text-secondary);
}
</style>
