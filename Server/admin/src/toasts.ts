import { reactive } from 'vue'

// 右下角的提示：几秒后自己消失
export interface Toast {
  id: number
  text: string
  bad: boolean
}

export const toasts = reactive<Toast[]>([])
let next = 0

export function toast(text: string, bad = false): void {
  const id = next++
  toasts.push({ id, text, bad })
  setTimeout(() => {
    const index = toasts.findIndex((t) => t.id === id)
    if (index >= 0) toasts.splice(index, 1)
  }, 3200)
}
