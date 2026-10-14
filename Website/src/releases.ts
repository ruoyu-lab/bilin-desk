import { reactive } from 'vue'

// 现在发布的版本：服务器的 /updates/<平台>/latest（程序检查更新也问它，Server/src/server.ts）。
// 网页和服务器放在一起，下载按钮直接指向它说的安装包
export type Platform = 'macos-arm64' | 'windows-x64' | 'android-arm64'

export interface Release {
  version: string
  size: number // 字节
  url: string
}

// loading：还在问；unpublished：这个平台还没发布过；unreachable：服务器连不上、出错了
export type ReleaseState = Release | 'loading' | 'unpublished' | 'unreachable'

export const releases = reactive<Record<Platform, ReleaseState>>({
  'macos-arm64': 'loading',
  'windows-x64': 'loading',
  'android-arm64': 'loading',
})

export async function loadReleases(): Promise<void> {
  await Promise.all(
    (Object.keys(releases) as Platform[]).map(async (platform) => {
      try {
        const response = await fetch(`/updates/${platform}/latest`, { cache: 'no-cache' })
        if (!response.ok) {
          releases[platform] = response.status === 404 ? 'unpublished' : 'unreachable'
          return
        }
        const { version, size, url } = (await response.json()) as Release
        releases[platform] = { version, size, url }
      } catch {
        releases[platform] = 'unreachable'
      }
    }),
  )
}

export function megabytes(release: Release): string {
  return `${(release.size / 1024 / 1024).toFixed(1)} MB`
}
