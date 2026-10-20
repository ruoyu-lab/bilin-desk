import {
  apple, bell, bolt, clock, copy, displays, download, file, globe, heart, home, monitor, people, plus, power, radar,
  refresh, search, server, settings, shield, star, wifi, windows, x
} from '@shared/icons'

// 本站用到的图标（WebShared/icons.ts 里取的）；components/LineIcon.vue 按名字画
export const icons = {
  apple,
  bell,
  bolt,
  clock,
  copy,
  displays,
  download,
  file,
  globe,
  heart,
  home,
  monitor,
  people,
  plus,
  power,
  radar,
  refresh,
  search,
  server,
  settings,
  shield,
  star,
  wifi,
  windows,
  x,
}

export type IconName = keyof typeof icons
