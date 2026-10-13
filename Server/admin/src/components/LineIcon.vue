<script setup lang="ts">
// 线条图标（24×24，描边随文字颜色），和官网的同一种画法（Website/src/components/LineIcon.vue）
export type IconName =
  | 'monitor' | 'chart' | 'package' | 'database' | 'user' | 'wifi' | 'upload' | 'download' | 'logout' | 'apple'
  | 'windows' | 'alert' | 'shield' | 'mail' | 'refresh'

withDefaults(defineProps<{ name: IconName; size?: number; stroke?: number }>(), { size: 20, stroke: 1.8 })

// 描边的
const paths: Partial<Record<IconName, string[]>> = {
  monitor: ['M5 4h14a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2z', 'M8 20h8', 'M12 16v4'],
  chart: ['M4 20V10', 'M10 20V4', 'M16 20v-7', 'M22 20H2'],
  package: ['M21 8 12 3 3 8v8l9 5 9-5z', 'm3 8 9 5 9-5', 'M12 13v8'],
  database: ['M20 5c0 1.7-3.6 3-8 3S4 6.7 4 5s3.6-3 8-3 8 1.3 8 3z', 'M4 5v14c0 1.7 3.6 3 8 3s8-1.3 8-3V5', 'M4 12c0 1.7 3.6 3 8 3s8-1.3 8-3'],
  user: ['M16 8a4 4 0 1 1-8 0 4 4 0 0 1 8 0z', 'M4 21c0-4 3.6-7 8-7s8 3 8 7'],
  wifi: ['M5 12.5a10 10 0 0 1 14 0', 'M8.5 16a5 5 0 0 1 7 0', 'M12 19.5h.01', 'M2 9a14.5 14.5 0 0 1 20 0'],
  upload: ['M12 15V3', 'm7 8 5-5 5 5', 'M5 21h14'],
  download: ['M12 3v12', 'm7 10 5 5 5-5', 'M5 21h14'],
  logout: ['M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4', 'm16 17 5-5-5-5', 'M21 12H9'],
  alert: ['M10.3 3.9 1.8 18a2 2 0 0 0 1.7 3h17a2 2 0 0 0 1.7-3L13.7 3.9a2 2 0 0 0-3.4 0z', 'M12 9v4', 'M12 17h.01'],
  shield: ['M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z', 'm9 12 2 2 4-4'],
  mail: ['M4 4h16a2 2 0 0 1 2 2v12a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2z', 'm22 6-10 7L2 6'],
  refresh: ['M21 12a9 9 0 0 1-15.5 6.2L3 16', 'M3 21v-5h5', 'M3 12a9 9 0 0 1 15.5-6.2L21 8', 'M21 3v5h-5'],
}

// 苹果、Windows 的标志是填充的
const filled: Partial<Record<IconName, string>> = {
  apple: 'M16.4 12.6c0-2.4 2-3.6 2.1-3.7-1.1-1.7-2.9-1.9-3.5-1.9-1.5-.2-2.9.9-3.7.9-.8 0-1.9-.9-3.2-.8-1.6 0-3.1 1-4 2.4-1.7 3-.4 7.4 1.2 9.8.8 1.2 1.8 2.5 3 2.4 1.2 0 1.7-.8 3.2-.8s1.9.8 3.2.8c1.3 0 2.2-1.2 3-2.4.9-1.4 1.3-2.7 1.3-2.8-.1 0-2.6-1-2.6-3.9zM14 5.5c.7-.8 1.1-1.9 1-3-1 0-2.1.7-2.8 1.5-.6.7-1.2 1.8-1 2.9 1 .1 2.1-.6 2.8-1.4z',
  windows: 'M3 5.5 10 4.6v6.9H3zM11 4.4 21 3v8.5H11zM3 12.5h7v6.9l-7-.9zM11 12.5h10V21l-10-1.4z',
}
</script>

<template>
  <svg v-if="filled[name]" :width="size" :height="size" viewBox="0 0 24 24" fill="currentColor" aria-hidden="true">
    <path :d="filled[name]" />
  </svg>
  <svg v-else :width="size" :height="size" viewBox="0 0 24 24" fill="none" stroke="currentColor" :stroke-width="stroke"
       stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">
    <path v-for="d in paths[name] ?? []" :key="d" :d="d" />
  </svg>
</template>
