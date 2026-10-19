import {
  apple, chevronDown, chevronRight, clipboard, clock, download, home, info, keyboard, logOut, maximize, monitor,
  palette, settings, star, upload, user, windows, x
} from '@shared/icons'

// 本站用到的图标（WebShared/icons.ts 里取的）；components/LineIcon.vue 按名字画
export const icons = {
  apple,
  'chevron-down': chevronDown,
  'chevron-right': chevronRight,
  clipboard,
  clock,
  download,
  home,
  info,
  keyboard,
  'log-out': logOut,
  maximize,
  monitor,
  palette,
  settings,
  star,
  upload,
  user,
  windows,
  x,
}

export type IconName = keyof typeof icons
