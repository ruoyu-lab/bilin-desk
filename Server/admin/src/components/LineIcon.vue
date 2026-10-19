<script setup lang="ts">
import BaseIcon from '@shared/LineIcon.vue'
import { icons, type IconName } from '../icons'

// 按名字画本站的一个图标（src/icons.ts）
withDefaults(defineProps<{ name: IconName; size?: number; stroke?: number }>(), { size: 20, stroke: 1.8 })
</script>

<template>
  <BaseIcon :icon="icons[name]" :size="size" :stroke="stroke" />
</template>
