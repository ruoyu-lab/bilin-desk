<script setup lang="ts">
import { computed } from 'vue'
import SiteFooter from '../components/SiteFooter.vue'
import SiteHeader from '../components/SiteHeader.vue'
import { parse } from '../legal/markdown'

// 服务协议、隐私政策：程序里的同一份文字（App/legal）
const props = defineProps<{ source: string }>()

const blocks = computed(() => parse(props.source))
</script>

<template>
  <SiteHeader />
  <main class="wrap document">
    <template v-for="(block, index) in blocks" :key="index">
      <h1 v-if="block.kind === 'title'">{{ block.text }}</h1>
      <h2 v-else-if="block.kind === 'heading'">{{ block.text }}</h2>
      <p v-else-if="block.kind === 'paragraph'">
        <template v-for="(part, at) in block.content" :key="at">
          <strong v-if="part.bold">{{ part.text }}</strong>
          <template v-else>{{ part.text }}</template>
        </template>
      </p>
      <ul v-else>
        <li v-for="(item, at) in block.items" :key="at">
          <template v-for="(part, partAt) in item" :key="partAt">
            <strong v-if="part.bold">{{ part.text }}</strong>
            <template v-else>{{ part.text }}</template>
          </template>
        </li>
      </ul>
    </template>
  </main>
  <SiteFooter />
</template>

<style scoped>
.document {
  max-width: 760px;
  padding-top: 52px;
  font-size: 15px;
  line-height: 1.85;
  color: var(--text-body);
}

h1 {
  margin-bottom: 20px;
  font-size: clamp(28px, 4vw, 36px);
  font-weight: 900;
  color: var(--text);
}

h2 {
  margin: 36px 0 10px;
  font-size: 19px;
  font-weight: 700;
  color: var(--text);
}

p {
  margin-bottom: 14px;
}

ul {
  margin: 0 0 14px;
  padding-left: 22px;
}

li {
  margin-bottom: 8px;
}

strong {
  color: var(--text);
}
</style>
