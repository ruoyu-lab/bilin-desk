# 手机版

手机和平板（先做 Android，iOS 之后）只去连电脑，不被连：iOS 不允许一个程序操作别的程序，Android 要无障碍服务、
上架审核严，以后再说。代码在 `mobile` 分支上做，测好了再合进 main。

## 和电脑版的关系

- **同一套代码**：`App/CMakeLists.txt` 把文件分成共用、电脑、手机三组。手机版（CMake 的 `BILIN_MOBILE`，Android、iOS
  上自动打开）用 `App/main_mobile.cpp` 和 `App/qml/mobile/`；组件、设置的子页（账号、关于比邻、协议）、登录表单（`LoginForm`）、
  会话的画面（`RemoteCanvas`）和侧边面板（对话、文件、设置）和电脑版共用
- **后台服务在程序的进程里**：手机上不能另起一个程序在后台跑。电脑上后台服务里"去连别人"的那部分（本机身份、账号、
  附近的设备、信令，`Service/src/service_wiring.h`）在手机上由 `LocalService`（`App/src/mobile/local_service.h`）
  在本进程里跑，界面照样经本机连接和它说话（`ServiceClient` 的 `InProcess`）。被别人连、系统服务、自己更新这些没有；
  别人来连手机一律回绝
- **手机不出现在电脑的列表里**：局域网里手机只听不说（`LanDiscovery` 的 `announce`），账号下的手机不在电脑的设备列表里
  （`rc::server::acceptsConnections`），账号页里照样能看到、能移除
- **尺寸**：`Theme.touch` 时按钮、输入框至少 44 高，字大一号（`Theme.controlHeight`、`bodyFont` 这些），电脑上不变

## 界面

- 窄的时候（手机、平板竖着）四个标签在底部：概览、设备、最近、设置；宽的时候（平板横着）在左边的侧栏
- 概览：连接到设备（设备 ID、IP 或名称）、我的设备（在线的在前，最多 4 台）、最近；设备：我的设备、附近、收藏三段，
  在线、离线分组，按住不放出菜单；设置：账号、本机名称、发现附近的设备、手指操作方式、连接时屏幕常亮、外观、关于比邻
- 第一次打开没登录时先到登录页（"以后再说"以后不再先进）

## 远程操作

画面铺满，左上角返回（断开）、名字和延迟，右上角键盘、手指操作方式、"…"（对话、文件、设置）。手指（`RemoteView` 的
`touchMode`）：

| 手势 | 触摸板（手机默认） | 直接点（平板默认） |
|---|---|---|
| 一个手指滑 | 移光标 | 按着左键拖 |
| 点一下 / 两下 | 在光标那里点击 / 双击 | 点哪就点哪 |
| 按住不放 | 按着左键，接着拖 | 右键 |
| 两个手指点一下 | 右键 | 右键 |
| 两个手指滑 | 滚轮（内容跟着手指走） | 同左 |
| 两个手指张开、捏拢 | 放大缩小画面，放大以后拖着移动 | 同左 |

- 放大以后光标走到边上画面跟着移；中间显示倍数，点一下复原
- 打字用 SDK 的文字输入（`RCS_INPUT_TEXT`，中文也行）；键盘上面一排 Esc、Tab、Ctrl、Alt、Win（对方是 Mac 时 ⌘ ⌥ ⌃）、
  方向键、F1~F12，修饰键点一下按住，打下一个键时一起按
- 画面：先用 SDK 从显卡读回来的 CPU 上的图像（电脑上解出的 GPU 画面直接画，`RC_GPU_FRAMES`）

## 编译、装到手机

要 Qt 的 Android 版（`~/Qt/6.11.2/android_arm64_v8a`）、Android SDK 和 NDK、JDK 17 起。连正式的服务器：

```sh
scripts/build-android.sh            # build/android/Bilin.apk，用调试密钥签名
scripts/build-android.sh --install  # 再装到 USB 连着的手机上并打开
```

在 Mac 上预览手机的界面（窗口里，尺寸按电脑的）：

```sh
cmake -S . -B build/mobile-preview -G Ninja -DCMAKE_PREFIX_PATH=$HOME/Qt/6.11.2/macos -DBILIN_MOBILE=ON -DBUILD_TESTING=OFF
cmake --build build/mobile-preview --target Bilin
```

Android 上的程序数据在程序自己的目录里（`rc::stateDirectory()`），设备令牌和私钥原样存（只有本程序能读，`user_seal_plain.cpp`）；
设备名是"设置 › 关于手机"里的名字，硬件编号是 ANDROID_ID（重装以后登录同一个账号接回原来的设备 ID）。
