<script setup lang="ts">
import { nextTick, onBeforeUnmount, ref, watch } from 'vue'
import wechatPay from '../assets/wechat-pay.jpg'
import LineIcon from './LineIcon.vue'
import type { IconName } from '../icons'

// 赞助的浮窗（页头的"赞助"）：赞助用在哪里、微信的收款码，电脑上左右两栏（收款码在左），手机上上下排。点外面、按 Esc、点右上角关掉
const open = defineModel<boolean>({ required: true })
const close = ref<HTMLButtonElement>()

const uses: { icon: IconName; title: string; text: string }[] = [
  { icon: 'server', title: '维护服务器', text: '设备之间互相找到、连不通时的中继转发，都靠服务器的机器和带宽。' },
  { icon: 'apple', title: '申请 Apple 开发者账号', text: '每年 688 元，有了它 iPhone、iPad 版才能上架 App Store。' },
  { icon: 'refresh', title: '维护客户端', text: 'macOS、Windows、Android、iOS 各平台的适配、修复和新功能。' },
]

function onKey(event: KeyboardEvent) {
  if (event.key === 'Escape')
    open.value = false
}

// 开着的时候页面不跟着滚，焦点放到关闭按钮上（键盘、读屏能直接用）
watch(open, async (value) => {
  document.body.style.overflow = value ? 'hidden' : ''
  if (value) {
    window.addEventListener('keydown', onKey)
    await nextTick()
    close.value?.focus()
  } else {
    window.removeEventListener('keydown', onKey)
  }
})

onBeforeUnmount(() => {
  window.removeEventListener('keydown', onKey)
  document.body.style.overflow = ''
})
</script>

<template>
  <!-- 挪到页面末尾自己的容器里（预渲染的页面被接管时，挪过去的东西要在一个单独的容器里认得出来） -->
  <Teleport to="#teleports">
    <div v-if="open" class="overlay" @click.self="open = false">
      <div class="dialog" role="dialog" aria-modal="true" aria-labelledby="sponsor-title">
        <button ref="close" class="close" aria-label="关闭" @click="open = false">
          <LineIcon name="x" :size="18" :stroke="2" />
        </button>
        <div class="text">
          <h2 id="sponsor-title">赞助比邻</h2>
          <p class="intro">比邻是免费、开源的个人项目，没有广告，也不卖数据。如果它帮到了你，欢迎随手赞助一点，让它走得更远。</p>
          <ul class="uses">
            <li v-for="use in uses" :key="use.title">
              <LineIcon :name="use.icon" :size="20" />
              <div>
                <strong>{{ use.title }}</strong>
                <span>{{ use.text }}</span>
              </div>
            </li>
          </ul>
        </div>
        <figure class="code">
          <img :src="wechatPay" width="240" height="268" alt="微信收款码" />
          <figcaption>微信扫一扫<span class="hint">，手机上可以长按保存图片</span></figcaption>
        </figure>
        <p class="note">赞助完全自愿，不影响任何功能的使用。谢谢你！</p>
      </div>
    </div>
  </Teleport>
</template>

<style scoped>
.overlay {
  position: fixed;
  inset: 0;
  z-index: 20;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 16px;
  background: rgba(0, 0, 0, 0.45);
}

.dialog {
  position: relative;
  box-sizing: border-box;
  display: grid;
  grid-template-columns: 248px 1fr;
  grid-template-areas:
    "code text"
    "code note";
  column-gap: 32px;
  align-items: center;
  width: 100%;
  max-width: 780px;
  max-height: 100%;
  overflow-y: auto;
  padding: 32px;
  border-radius: 18px;
  background: var(--bg);
  box-shadow: 0 24px 64px rgba(0, 0, 0, 0.25);
}

.text {
  grid-area: text;
}

.close {
  position: absolute;
  top: 14px;
  right: 14px;
  display: flex;
  padding: 7px;
  border: none;
  border-radius: 8px;
  background: none;
  color: var(--text-muted);
  cursor: pointer;
}

.close:hover {
  background: var(--fill);
  color: var(--text);
}

h2 {
  font-size: 22px;
  font-weight: 800;
}

.intro {
  margin-top: 10px;
  font-size: 14px;
  line-height: 1.7;
  color: var(--text-secondary);
}

.uses {
  display: flex;
  flex-direction: column;
  gap: 14px;
  margin: 20px 0 0;
  padding: 0;
  list-style: none;
}

.uses li {
  display: flex;
  gap: 12px;
  align-items: flex-start;
}

.uses li > :first-child {
  flex: none;
  margin-top: 1px;
}

.uses div {
  display: flex;
  flex-direction: column;
  gap: 3px;
}

.uses strong {
  font-size: 14px;
  font-weight: 600;
}

.uses span {
  font-size: 13px;
  line-height: 1.6;
  color: var(--text-secondary);
}

.code {
  grid-area: code;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 8px;
  margin: 0;
  padding: 16px;
  border-radius: 14px;
  background: var(--card);
}

/* 收款码是白底的：深色时也放在白色的卡片里扫 */
.code img {
  width: 200px;
  height: auto;
  padding: 8px;
  border-radius: 10px;
  background: #ffffff;
}

.code figcaption {
  font-size: 13px;
  color: var(--text-muted);
}

/* 长按保存只对手机说 */
.hint {
  display: none;
}

.note {
  grid-area: note;
  margin-top: 16px;
  font-size: 12px;
  color: var(--text-muted);
}

/* 手机上上下排：说明、收款码、最后一句 */
@media (max-width: 680px) {
  .dialog {
    grid-template-columns: 1fr;
    grid-template-areas:
      "text"
      "code"
      "note";
    max-width: 440px;
    padding: 28px 28px 24px;
  }

  .code {
    margin-top: 22px;
  }

  .note {
    text-align: center;
  }

  .hint {
    display: inline;
  }
}

/* 深色时页面本来就暗：遮罩更深，浮窗亮一档，分得开 */
@media (prefers-color-scheme: dark) {
  .overlay {
    background: rgba(0, 0, 0, 0.6);
  }

  .dialog {
    background: var(--sidebar);
  }
}
</style>
