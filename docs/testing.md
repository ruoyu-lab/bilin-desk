# 测试

## 自动测试（ctest）

```sh
ctest --test-dir build/debug --output-on-failure
```

Mac 和 Windows 上各一套，不用服务器、不用 SDK 的真连接（用到 SDK 的部分只在程序里，测试不链接）：

| | 测什么 |
|---|---|
| Core/tests | 设备 ID、临时密码、本机连接上的消息、和服务器 / 局域网之间的消息、对话记录、传输列表和没传完的记录、数据目录（`RC_STATE_DIR`）、设备令牌和私钥的加密存放、新版本的签名（和服务器、签名脚本签的对得上） |
| Service/tests | 本机身份、本机连接的服务端、和服务器之间的连接（含 wss 不认系统不信任的证书）、信令往哪走、局域网信令、两个分享会话的编号、界面程序意外没了时的处理、新版本（假的服务器；设了 `RC_UPDATE_SERVER` 时还从真的服务器下载一次） |
| App/tests | 和一台设备的对话、互传文件的列表、远程声音的说明、设备列表的筛选（Qt Quick Test）、安装卸载的步骤 |
| Server/test（`npm test`） | 协议、设备登记、账号、限速、中继凭据、多实例转发、管理后台（登录、备份、发布）、新版本的签名和下载。不需要 MongoDB、Redis |

## 端到端（scripts/e2e）

真的进程、真的服务器、真的 SDK 连接。被连的电脑是一个**隔离的后台服务**（环境变量 `RC_STATE_DIR`：自己的数据目录、
自己的本机连接名，不碰正在用的那份），去连的是命令行的 `rcconnect`。只在 macOS 上跑（要屏幕录制的授权，
测试只数收到多少帧画面，不存画面）。测试用的账号测完注销，开的进程测完关掉，数据目录测完删掉。

```sh
scripts/e2e/passwordless.py                              # 开发用的服务器（build/debug）
E2E_BUILD=build/production E2E_EMAIL=… E2E_PASSWORD=… scripts/e2e/passwordless.py
    # 正式的服务器（安装包用的那份构建）：验证码会真的发到邮箱，用自己先注册好的测试账号（测完只退出、不注销）
scripts/e2e/resume.py                                    # 互传文件的断点续传
E2E_WINDOWS=用户@主机 E2E_WINDOWS_DIR='C:\\...\\Bilin' E2E_WINDOWS_QT='F:\\Qt\\6.11.2\\msvc2022_64\\bin' \
    scripts/e2e/windows.py                               # Windows 去连 Mac（经 SSH 跑 Windows 上的 rcconnect）
E2E_WINDOWS_VM='Windows 11' E2E_WINDOWS_DIR='C:\\src' E2E_WINDOWS_BUILD=bilin-build \
    E2E_WINDOWS_QT='C:\\Qt\\6.11.2\\msvc2022_arm64\\bin' scripts/e2e/windows.py   # 或者这台 Mac 上的 Parallels 虚拟机（prlctl exec）
```

- `passwordless.py`（26 项）：注册账号、改昵称、改密码要旧密码；**我的设备**按设备密钥连上（不要密码、不用点允许、能控制）；
  不认识的设备拿着公钥来被拒；**别人**要临时密码、要点允许（只能看）、密码错被拒、没有界面时被拒；
  关着"允许别人连接本机"时别人被回绝、我的设备照样连；被移出账号的设备不能再免密码连；
  连接记录（铃铛里的通知）记着连过的、没成的、输错密码的；注销账号
- `resume.py`（17 项）：发 300MB 随机文件，收到约 40MB 时把发送端 `kill -9`，等被连的一方发现断了（SDK 等对方回来
  一分钟），再启动发送端：接着传（不是从头）、存回上次的文件夹、最后文件的 sha256 和原来一样、半截文件和记录都清掉

- `windows.py`（11 项）：Mac 上隔离的后台服务当被连的电脑，Windows 上的 rcconnect 去连：我的设备免密码、能控制、收到画面；
  不认识的设备被拒；别人要点允许；Windows 发 30MB 文件给 Mac，大小和 sha256 对得上；Windows 上的设备令牌和私钥是
  DPAPI 加密存的。Windows 上只用 `E2E_WINDOWS_DIR` 下的 `e2e-state` 一个目录，测完删掉。要在 Windows 上先编好 rcconnect
  （`scripts\\build-windows.cmd`），并且那台电脑上没有别的程序在用（先看任务管理器）

注意：

- **要屏幕亮着、没锁屏**：被连的电脑要采集屏幕，显示器关了或者锁屏了采集不了，连得上也没有画面、会话直接失败。
  脚本看到这种情况会说"这次结果不算"（退出码 2），不是程序的问题。人不在时显示器会自动关，所以别人不在电脑前时没法跑。
- 连着被连的电脑的"界面"（脚本里的本机连接）断开、而又有人连着时，后台服务会把界面程序拉起来（有人连着本机时界面程序要在），
  在这台电脑上弹出刘海。脚本按顺序收拾，不会触发；手动试的时候先关去连的，再关后台服务。
- 服务器限速：同一个地址一小时最多登记 20 台新设备，每次端到端测试用掉 6~7 台。开发用的虚拟机上超了可以清掉计数：
  `orb -m bilin-dev bash -c 'redis-cli --scan --pattern "bilin:limit:*" | xargs redis-cli del'`。
  在正式的服务器上测完要删掉测试留下的设备（MongoDB 的 `devices`，按名字）。

## 只能人在电脑前测的

这些要界面或管理员密码，没法脚本化，写在测试清单里让人测：

- 两个安装包：Windows 的 `Bilin-Setup.exe`（装、升级、"应用和功能"里卸载）、macOS 的 `Bilin.pkg`
  （装、"设置 › 关于比邻"最下面的卸载）
- macOS 第一次打开的授权引导、屏幕录制授权被收回后概览页的提示
- 界面：设置 › 账号（改昵称、改密码、移除别的电脑、退出、注销）、铃铛里的连接记录、收文件时问"想接着发上次没发完的"的卡片、
  退出前的确认、开机自启
- Windows 上被连：采集在 Windows 上和 Mac 上走的是同一份代码，但 DPAPI 加密身份、防火墙、托盘只有真机上才看得到
