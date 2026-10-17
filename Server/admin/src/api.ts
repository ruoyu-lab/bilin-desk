import { ref } from 'vue'

// 管理后台的接口（Server/src/admin/server.ts）：在网页所在目录下的 api/（正式服务器上是 /admin/api/）
export const base = location.pathname.replace(/[^/]*$/, '')

// 登录了没有：null 是还在问
export const signedIn = ref<boolean | null>(null)

export class ApiError extends Error {
  constructor(
    readonly code: string | null,
    readonly status: number,
    // 服务端说的原因（比如测试邮件没发出去时 Resend 回的话）
    readonly detail = '',
  ) {
    super(code ?? String(status))
  }
}

// 没登录（401）时回到登录页
export async function api<T>(method: string, path: string, body?: unknown): Promise<T> {
  const response = await fetch(`${base}api/${path}`, {
    method,
    credentials: 'same-origin',
    headers: body === undefined ? {} : { 'content-type': 'application/json' },
    body: body === undefined ? undefined : JSON.stringify(body),
  })
  if (response.status === 401 && path !== 'login') {
    signedIn.value = false
    throw new ApiError('signedOut', 401)
  }
  const data = response.status === 204 ? null : await response.json().catch(() => null)
  if (!response.ok) throw new ApiError(data?.error ?? null, response.status, data?.message ?? '')
  return data as T
}

// 下面几个和服务端的一样（Server/src/admin/status.ts、logs.ts、backups.ts，Server/src/releases.ts、feedback.ts）

export interface Status {
  uptimeS: number
  memory: { totalBytes: number; availableBytes: number }
  disk: { totalBytes: number; availableBytes: number }
  databaseBytes: number
  devices: { registered: number; online: number }
  versions: Array<{ platform: string; version: string; devices: number }>
  accounts: number
  services: Record<string, boolean>
  system: { os: string; kernel: string; cpus: number; load: number[]; node: string; mongodb: string; redis: string }
  certificate: { host: string; validTo: string | null } | null
  backup: Backup | null
  mail: {
    mode: 'resend' | 'log' | 'none'
    today: number
    limit: number
    lastFailure: { at: string; reason: string } | null
  }
  trends: Array<{ day: string; devices: number; accounts: number; connections: number; peakOnline: number }>
}

export interface LogEntry {
  time: string
  level: 'warn' | 'error'
  app: string
  message: string
  detail: string
}

export interface Feedback {
  id: string
  createdAt: string
  deviceId: string
  deviceName: string
  platform: string
  version: string
  system: string
  contact: string
  signedIn: boolean
  text: string
  logsBytes: number
  read: boolean
}

export interface Release {
  platform: string
  version: string
  size: number
  sha256: string
  signature: string
  file: string
  publishedAt: string
}

export interface PlatformReleases {
  platform: string
  current: string | null
  releases: Release[]
}

export interface Backup {
  name: string
  size: number
  createdAt: string
}

// 签名文件（scripts/sign-release.mjs 写的 <安装包>.release.json）
export interface SignedRelease {
  platform: string
  version: string
  size: number
  signature: string
}

export const platforms: Record<string, { name: string; icon: 'apple' | 'windows' | 'phone' }> = {
  'macos-arm64': { name: 'macOS', icon: 'apple' },
  'windows-x64': { name: 'Windows', icon: 'windows' },
  'android-arm64': { name: 'Android', icon: 'phone' },
}

export function platformName(platform: string): string {
  return platforms[platform]?.name ?? platform
}
