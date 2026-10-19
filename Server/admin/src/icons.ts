import {
  alert, apple, chart, database, download, logOut, mail, message, monitor, packageIcon, phone, refresh, shield, trash,
  upload, user, wifi, windows
} from '@shared/icons'

// 本站用到的图标（WebShared/icons.ts 里取的）；components/LineIcon.vue 按名字画
export const icons = {
  alert,
  apple,
  chart,
  database,
  download,
  'log-out': logOut,
  mail,
  message,
  monitor,
  package: packageIcon,
  phone,
  refresh,
  shield,
  trash,
  upload,
  user,
  wifi,
  windows,
}

export type IconName = keyof typeof icons
