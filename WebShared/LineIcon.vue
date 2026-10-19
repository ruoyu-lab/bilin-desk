<script setup lang="ts">
import type { Icon } from './icons'

// 画一个线条图标（icons.ts）：描边的跟着文字颜色描，苹果、Windows 的标志是填充的。
// 各个网站的 components/LineIcon.vue 按名字从自己的 icons.ts 里取了交给它
withDefaults(defineProps<{ icon: Icon; size?: number; stroke?: number }>(), { size: 24, stroke: 1.8 })
</script>

<template>
  <svg v-if="'fill' in icon" :width="size" :height="size" viewBox="0 0 24 24" fill="currentColor" aria-hidden="true">
    <path :d="icon.fill" />
  </svg>
  <svg v-else :width="size" :height="size" viewBox="0 0 24 24" fill="none" stroke="currentColor" :stroke-width="stroke"
       stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">
    <path v-for="d in icon.paths" :key="d" :d="d" />
  </svg>
</template>
