<script setup lang="ts">
// 程序的 Logo（App/branding/logo.svg）：白色圆角方块正中一支黑色指针，两道浅灰的残影从右下角拉过来
defineProps<{ size: number }>()

const pointer = 'M450 330 L450 690 L536 606 L592 730 L652 703 L596 581 L716 581 Z'
</script>

<template>
  <svg :width="size" :height="size" viewBox="100 100 824 824" aria-hidden="true">
    <rect x="101.5" y="101.5" width="821" height="821" rx="184.5" fill="#ffffff" stroke="#000000" stroke-opacity="0.1" stroke-width="12" />
    <path transform="translate(79 132)" :d="pointer" fill="#e6e8ec" stroke="#e6e8ec" stroke-width="20" stroke-linejoin="round" />
    <path transform="translate(4 57)" :d="pointer" fill="#c3c7ce" stroke="#c3c7ce" stroke-width="20" stroke-linejoin="round" />
    <path transform="translate(-71 -18)" :d="pointer" fill="#16171a" stroke="#16171a" stroke-width="20" stroke-linejoin="round" />
  </svg>
</template>
