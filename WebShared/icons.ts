// 线条图标（24×24，描边随文字颜色；Lucide 的画法，程序里用的同一套）。每个图标单独导出，
// 各个网站在自己的 src/icons.ts 里只引用用到的那几个，打包时没用到的不带
export type Icon = { paths: string[] } | { fill: string }

export const alert: Icon = { paths: ['M10.3 3.9 1.8 18a2 2 0 0 0 1.7 3h17a2 2 0 0 0 1.7-3L13.7 3.9a2 2 0 0 0-3.4 0z', 'M12 9v4', 'M12 17h.01'] }
// 填充的标志
export const apple: Icon = { fill: 'M16.4 12.6c0-2.4 2-3.6 2.1-3.7-1.1-1.7-2.9-1.9-3.5-1.9-1.5-.2-2.9.9-3.7.9-.8 0-1.9-.9-3.2-.8-1.6 0-3.1 1-4 2.4-1.7 3-.4 7.4 1.2 9.8.8 1.2 1.8 2.5 3 2.4 1.2 0 1.7-.8 3.2-.8s1.9.8 3.2.8c1.3 0 2.2-1.2 3-2.4.9-1.4 1.3-2.7 1.3-2.8-.1 0-2.6-1-2.6-3.9zM14 5.5c.7-.8 1.1-1.9 1-3-1 0-2.1.7-2.8 1.5-.6.7-1.2 1.8-1 2.9 1 .1 2.1-.6 2.8-1.4z' }
export const bell: Icon = { paths: ['M10.3 21a1.9 1.9 0 0 0 3.4 0', 'M3.3 15.3A1 1 0 0 0 4 17h16a1 1 0 0 0 .7-1.7C19.4 14 18 12.6 18 8A6 6 0 0 0 6 8c0 4.6-1.4 6-2.7 7.3'] }
export const bolt: Icon = { paths: ['M13 2 4 14h7l-1 8 9-12h-7z'] }
export const chart: Icon = { paths: ['M4 20V10', 'M10 20V4', 'M16 20v-7', 'M22 20H2'] }
export const chevronDown: Icon = { paths: ['m6 9 6 6 6-6'] }
export const chevronRight: Icon = { paths: ['m9 18 6-6-6-6'] }
export const clipboard: Icon = { paths: ['M9 2h6a1 1 0 0 1 1 1v2a1 1 0 0 1-1 1H9a1 1 0 0 1-1-1V3a1 1 0 0 1 1-1z', 'M16 4h2a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2h2'] }
export const clock: Icon = { paths: ['M22 12a10 10 0 1 1-20 0 10 10 0 0 1 20 0z', 'M12 6v6l4 2'] }
export const copy: Icon = { paths: ['M11 9h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2h-8a2 2 0 0 1-2-2v-8a2 2 0 0 1 2-2z', 'M5 15V5a2 2 0 0 1 2-2h10'] }
export const database: Icon = { paths: ['M20 5c0 1.7-3.6 3-8 3S4 6.7 4 5s3.6-3 8-3 8 1.3 8 3z', 'M4 5v14c0 1.7 3.6 3 8 3s8-1.3 8-3V5', 'M4 12c0 1.7 3.6 3 8 3s8-1.3 8-3'] }
export const displays: Icon = { paths: ['M3.5 4h10a1.5 1.5 0 0 1 1.5 1.5v6a1.5 1.5 0 0 1-1.5 1.5h-10A1.5 1.5 0 0 1 2 11.5v-6A1.5 1.5 0 0 1 3.5 4z', 'M10.5 11h10a1.5 1.5 0 0 1 1.5 1.5v6a1.5 1.5 0 0 1-1.5 1.5h-10A1.5 1.5 0 0 1 9 18.5v-6a1.5 1.5 0 0 1 1.5-1.5z'] }
export const download: Icon = { paths: ['M12 3v12', 'm7 10 5 5 5-5', 'M5 21h14'] }
export const file: Icon = { paths: ['M14 3H6a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V9z', 'M14 3v6h6', 'M12 12v6', 'm9 15 3 3 3-3'] }
export const globe: Icon = { paths: ['M22 12a10 10 0 1 1-20 0 10 10 0 0 1 20 0z', 'M12 2a14.5 14.5 0 0 0 0 20 14.5 14.5 0 0 0 0-20', 'M2 12h20'] }
export const heart: Icon = { paths: ['M19 14c1.5-1.5 3-3.2 3-5.5A5.5 5.5 0 0 0 16.5 3c-1.8 0-3 .5-4.5 2-1.5-1.5-2.7-2-4.5-2A5.5 5.5 0 0 0 2 8.5c0 2.3 1.5 4 3 5.5l7 7z'] }
export const home: Icon = { paths: ['M15 21v-8a1 1 0 0 0-1-1h-4a1 1 0 0 0-1 1v8', 'M3 10a2 2 0 0 1 .7-1.5l7-6a2 2 0 0 1 2.6 0l7 6A2 2 0 0 1 21 10v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z'] }
export const info: Icon = { paths: ['M22 12a10 10 0 1 1-20 0 10 10 0 0 1 20 0z', 'M12 16v-4', 'M12 8h.01'] }
export const keyboard: Icon = { paths: ['M4 5h16a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V7a2 2 0 0 1 2-2z', 'M10 9h.01', 'M14 9h.01', 'M18 9h.01', 'M6 9h.01', 'M8 13h.01', 'M12 13h.01', 'M16 13h.01', 'M7 16h10'] }
export const logOut: Icon = { paths: ['M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4', 'm16 17 5-5-5-5', 'M21 12H9'] }
export const mail: Icon = { paths: ['M4 4h16a2 2 0 0 1 2 2v12a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2z', 'm22 6-10 7L2 6'] }
export const maximize: Icon = { paths: ['M8 3H5a2 2 0 0 0-2 2v3', 'M21 8V5a2 2 0 0 0-2-2h-3', 'M3 16v3a2 2 0 0 0 2 2h3', 'M16 21h3a2 2 0 0 0 2-2v-3'] }
export const message: Icon = { paths: ['M21 15a2 2 0 0 1-2 2H7l-4 4V5a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2z'] }
export const monitor: Icon = { paths: ['M5 4h14a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2z', 'M8 20h8', 'M12 16v4'] }
// package 是保留字，不能当名字
export const packageIcon: Icon = { paths: ['M21 8 12 3 3 8v8l9 5 9-5z', 'm3 8 9 5 9-5', 'M12 13v8'] }
export const palette: Icon = { paths: ['M12 22a10 10 0 1 1 10-10c0 2.2-1.8 3-3.5 3H17a2 2 0 0 0-1.5 3.3A1.9 1.9 0 0 1 12 22z', 'M13.5 6.5h.01', 'M17.5 10.5h.01', 'M8.5 7.5h.01', 'M6.5 12.5h.01'] }
export const people: Icon = { paths: ['M12.5 8a3.5 3.5 0 1 1-7 0 3.5 3.5 0 0 1 7 0z', 'M3 20c0-3.3 2.7-6 6-6s6 2.7 6 6', 'M16 4.5a3.5 3.5 0 0 1 0 7', 'M18 14c1.9.7 3 2.9 3 6'] }
export const phone: Icon = { paths: ['M7 2h10a2 2 0 0 1 2 2v16a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2z', 'M12 18h.01'] }
export const plus: Icon = { paths: ['M5 12h14', 'M12 5v14'] }
export const power: Icon = { paths: ['M12 2v10', 'M18.4 6.6a9 9 0 1 1-12.8 0'] }
export const radar: Icon = { paths: ['M19.07 4.93A10 10 0 0 0 6.99 3.34', 'M4 6h.01', 'M2.29 9.62A10 10 0 1 0 21.31 8.35', 'M16.24 7.76A6 6 0 1 0 8.23 16.67', 'M12 18h.01', 'M17.99 11.66A6 6 0 0 1 15.77 16.67', 'M14 12a2 2 0 1 1-4 0 2 2 0 0 1 4 0z', 'm13.41 10.59 5.66-5.66'] }
export const refresh: Icon = { paths: ['M3 12a9 9 0 0 1 15-6.7L21 8', 'M21 3v5h-5', 'M21 12a9 9 0 0 1-15 6.7L3 16', 'M8 16H3v5'] }
export const search: Icon = { paths: ['M19 11a8 8 0 1 1-16 0 8 8 0 0 1 16 0z', 'm21 21-4.3-4.3'] }
export const server: Icon = { paths: ['M4 2h16a2 2 0 0 1 2 2v4a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2z', 'M4 14h16a2 2 0 0 1 2 2v4a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2v-4a2 2 0 0 1 2-2z', 'M6 6h.01', 'M6 18h.01'] }
export const settings: Icon = { paths: ['M12.2 2h-.4a2 2 0 0 0-2 2v.2a2 2 0 0 1-1 1.7l-.4.3a2 2 0 0 1-2 0l-.2-.1a2 2 0 0 0-2.7.7l-.2.4a2 2 0 0 0 .7 2.7l.2.1a2 2 0 0 1 1 1.7v.5a2 2 0 0 1-1 1.7l-.2.1a2 2 0 0 0-.7 2.7l.2.4a2 2 0 0 0 2.7.7l.2-.1a2 2 0 0 1 2 0l.4.3a2 2 0 0 1 1 1.7v.2a2 2 0 0 0 2 2h.4a2 2 0 0 0 2-2v-.2a2 2 0 0 1 1-1.7l.4-.3a2 2 0 0 1 2 0l.2.1a2 2 0 0 0 2.7-.7l.2-.4a2 2 0 0 0-.7-2.7l-.2-.1a2 2 0 0 1-1-1.7v-.5a2 2 0 0 1 1-1.7l.2-.1a2 2 0 0 0 .7-2.7l-.2-.4a2 2 0 0 0-2.7-.7l-.2.1a2 2 0 0 1-2 0l-.4-.3a2 2 0 0 1-1-1.7V4a2 2 0 0 0-2-2z', 'M15 12a3 3 0 1 1-6 0 3 3 0 0 1 6 0z'] }
export const shield: Icon = { paths: ['M20 13c0 5-3.5 7.5-7.7 8.9a1 1 0 0 1-.6 0C7.5 20.5 4 18 4 13V6a1 1 0 0 1 1-1c2 0 4.5-1.2 6.2-2.7a1.2 1.2 0 0 1 1.6 0C14.5 3.8 17 5 19 5a1 1 0 0 1 1 1z', 'm9 12 2 2 4-4'] }
export const star: Icon = { paths: ['M11.5 2.3a.5.5 0 0 1 1 0l2.3 4.7a2 2 0 0 0 1.6 1.1l5.2.8a.5.5 0 0 1 .3.9l-3.8 3.7a2 2 0 0 0-.6 1.9l.9 5.2a.5.5 0 0 1-.8.6l-4.6-2.5a2 2 0 0 0-2 0l-4.6 2.5a.5.5 0 0 1-.8-.6l.9-5.2a2 2 0 0 0-.6-1.9L1.9 9.8a.5.5 0 0 1 .3-.9l5.2-.8A2 2 0 0 0 9 7z'] }
export const trash: Icon = { paths: ['M3 6h18', 'M8 6V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2', 'M19 6l-1 14a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2L5 6'] }
export const upload: Icon = { paths: ['M12 15V3', 'm7 8 5-5 5 5', 'M5 21h14'] }
export const user: Icon = { paths: ['M16 8a4 4 0 1 1-8 0 4 4 0 0 1 8 0z', 'M4 21c0-4 3.6-7 8-7s8 3 8 7'] }
export const wifi: Icon = { paths: ['M5 12.5a10 10 0 0 1 14 0', 'M8.5 16a5 5 0 0 1 7 0', 'M12 19.5h.01', 'M2 9a14.5 14.5 0 0 1 20 0'] }
// 填充的标志
export const windows: Icon = { fill: 'M3 5.5 10 4.6v6.9H3zM11 4.4 21 3v8.5H11zM3 12.5h7v6.9l-7-.9zM11 12.5h10V21l-10-1.4z' }
export const x: Icon = { paths: ['M18 6 6 18', 'm6 6 12 12'] }
