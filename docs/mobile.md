# 手机版

手机和平板（Android、iPhone 和 iPad）只去连电脑，不被连：iOS 不允许一个程序操作别的程序，Android 要无障碍服务、
上架审核严，以后再说。

## 和电脑版的关系

- **同一套代码**：`App/CMakeLists.txt` 把文件分成共用、电脑、手机三组。手机版（CMake 的 `BILIN_MOBILE`，Android、iOS
  上自动打开）用 `App/main_mobile.cpp` 和 `App/qml/mobile/`；组件、设置的子页（账号、关于比邻、协议）、登录表单（`LoginForm`）、
  会话的画面（`RemoteCanvas`）和侧边面板（对话、文件、设置）和电脑版共用
- **后台服务在程序的进程里**：手机上不能另起一个程序在后台跑。电脑上后台服务里"去连别人"的那部分（本机身份、账号、
  附近的设备、信令，`Service/src/service_wiring.h`）在手机上由 `LocalService`（`App/src/mobile/local_service.h`）
  在本进程里跑，界面照样经本机连接和它说话（`ServiceClient` 的 `InProcess`）。被别人连、系统服务这些没有，
  别人来连手机一律回绝；新版本照样从服务器来（下面的"更新"）
- **手机不出现在电脑的列表里**：局域网里手机只听不说（`LanDiscovery` 的 `announce`），账号下的手机不在电脑的设备列表里
  （`rc::server::acceptsConnections`），账号页里照样能看到、能移除
- **尺寸**：`Theme.touch` 时按钮、输入框至少 44 高，字大一号（`Theme.controlHeight`、`bodyFont` 这些），电脑上不变

## 界面

- 窄的时候（手机、平板竖着）四个标签在底部：概览、设备、最近、设置；宽的时候（平板横着）在左边的侧栏
- 概览：连接到设备（设备 ID、IP 或名称）、我的设备（在线的在前，最多 4 台）、最近；设备：我的设备、附近、收藏三段，
  在线、离线分组，按住不放出菜单；设置：账号、本机名称、发现附近的设备、手指操作方式、连接时屏幕常亮、外观、关于比邻
- 第一次打开没登录时先到登录页（"以后再说"以后不再先进）

## 远程操作

画面铺满，左上角返回、名字和延迟（点名字出对方电脑的电源菜单：锁定、注销、重启、关机），右上角键盘、快捷键
（复制、粘贴、任务管理器这些，`RemoteCanvas` 的 `shortcuts`）、手指操作方式、"…"（对话、文件、设置）。返回、
系统的返回键先问一句再断开。手指（`RemoteView` 的 `touchMode`）：

| 手势 | 触摸板（手机默认） | 直接点（平板默认） |
|---|---|---|
| 一个手指滑 | 移光标 | 按着左键拖 |
| 点一下 / 两下 | 在光标那里点击 / 双击 | 点哪就点哪 |
| 按住不放 | 按着左键，接着拖 | 右键 |
| 两个手指点一下 | 右键 | 右键 |
| 两个手指滑 | 滚轮（内容跟着手指走） | 同左 |
| 两个手指张开、捏拢 | 放大缩小画面，放大以后拖着移动 | 同左 |

- 放大以后光标走到边上画面跟着移；中间显示倍数，点一下复原
- 键盘按钮弹出菜单：打字（系统键盘，用 SDK 的文字输入 `RCS_INPUT_TEXT`，中文也行），或者按键栏（底下一排 Esc、Tab、
  Ctrl、Alt、Win（对方是 Mac 时 ⌘ ⌥ ⌃）、方向键、F1~F12，不弹系统键盘、不挡着），修饰键点一下按住，打下一个键时一起按
- 按住不放（右键、开始拖）时震一下（`PhoneWindow.pressFeedback`）；连着的时候屏幕常亮（设置里能关）
- 外接的键盘鼠标（蓝牙、USB、iPad 的妙控键盘）直接用：Qt 在手机上只给是哪个键，按美式键盘的位置对到 USB 的键码
  （`remote_input.cpp`；iPad 的 Command、Control 按对方是不是 Mac 对调，和 Mac 电脑版一样），鼠标是真的光标，不走
  触摸板那一套
- 画面：解码出来的帧不读回内存，直接画（Android 做成 OpenGL ES 的外部纹理，`frame_textures_android.cpp`、
  `shaders/external_gles.frag`；iOS 和 Mac 一样经 Metal，`frame_textures_mac.mm`）
- 互传文件：选文件用系统的文件选择（Android 是 `AndroidFilePicker`，可以多选），选出来的地址 SDK 读不了，先复制到
  程序的缓存里再发（`mobile_files.h`），不能发文件夹。收到的 Android 放在"下载/比邻"（Android 10 是程序自己的目录），
  点"打开"用别的程序打开；iOS 放在"文件"App 的"我的 iPad › 比邻"，点"打开"是快速预览（`ios_files.h`）

## 切到后台

Qt 在 Android 上程序到了后台（onStop）时停住所有事件循环，系统过一会儿还会冻结整个进程，连接就断了。所以有会话时开着
前台服务（`SessionService.java`，通知栏里"正在远程连接"，点了回到比邻；从任务列表里划掉比邻时跟着停）：进程不被冻结，
SDK 自己的线程照样维持连接，界面那边的事（消息、文件的进度）回来时再处理。程序不在前面（切走、锁屏、系统的文件选择
盖住）时会话暂停画面（`rcs_session_pause`，对方也不再给这台手机采集、编码），回来时从关键帧接着；远程声音开着的
照样听得到（`App/src/mobile/session_keeper.h`）。Android 13 起发通知要用户允许：第一次连接时问一次（只问这一次），
不给也行，通知栏里看不到这条，连接照样不断。

iOS 不让程序在后台一直跑：SDK 在程序到了后台时自己暂停画面（`rcs_background_config`），切出去一小会儿连接还在，
时间长了程序被系统挂起，连接断开，回来时重连（我的设备不用密码；别人的电脑临时密码用过就换，要重新输）。

## 编译、装到手机

要 Qt 的 Android 版（`~/Qt/6.11.2/android_arm64_v8a`）、Android SDK 和 NDK、JDK 17 起，还有 Android SDK 目录下的
`android_openssl`（KDAB，Qt Creator 装 Android 开发环境时一起下好）：Qt 在 Android 上的加密连接靠 OpenSSL，Qt 不带，
打包时放进 APK（开源声明里有它）。连正式的服务器：

```sh
scripts/build-android.sh new-key    # 第一次：生成签 APK 的密钥库（~/.bilin/android-release.keystore）
scripts/build-android.sh            # build/android/Bilin.apk
scripts/build-android.sh --install  # 再装到 USB 连着的手机上并打开
```

APK 用 `~/.bilin/android-release.keystore`（密码在旁边的 `android-release.password`）签名，不进仓库，**要备份**：
Android 只让签名一样的 APK 覆盖安装，丢了已经装着的手机就收不到新版本，只能卸载重装。以前用调试密钥签的装着的，
要先卸载（`adb uninstall com.bilindesk.bilin`）。

iOS（iPhone、iPad，iOS 17 起）：要 Xcode（"设置 › 帐户"里登录 Apple ID）和 Qt 的 iOS 版（`~/Qt/6.11.2/ios`）。
设备第一次装之前打开"设置 › 隐私与安全性 › 开发者模式"，装上以后在"设置 › 通用 › VPN 与设备管理"里信任自己的
开发者证书：

```sh
scripts/build-ios.sh            # build/ios/App/Release-iphoneos/Bilin.app
scripts/build-ios.sh --install  # 再装到 USB 连着的 iPad、iPhone 上并打开
```

签名用 Xcode 里登录的账号。免费的个人团队签的装上 7 天有效，程序的标识用 `com.bilindesk.bilin.dev`：正式的
`com.bilindesk.bilin` 留给开发者账号（个人团队登记过的标识，别的团队就用不了了）。iOS 上的不同：

- 局域网里发现附近的电脑靠 UDP 广播，iOS 上要向苹果另外申请组播权限，没有时"附近"是空的；按设备 ID、IP 连不受影响
  （第一次连局域网地址时系统问能不能用本地网络，`Info.plist` 的 `NSLocalNetworkUsageDescription`）
- 设备名 iOS 16 起只给"iPad""iPhone"（真名要苹果另批的权限），可以在设置里改；硬件编号没有，第一次用时生成一个存在
  钥匙串里（`machine_ios.mm`，删掉重装还在）
- 新版本从 App Store 来，程序里不检查更新

在 Mac 上预览手机的界面（窗口里，尺寸按电脑的）：

```sh
cmake -S . -B build/mobile-preview -G Ninja -DCMAKE_PREFIX_PATH=$HOME/Qt/6.11.2/macos -DBILIN_MOBILE=ON -DBUILD_TESTING=OFF
cmake --build build/mobile-preview --target Bilin
```

## 更新

从官网装的 Android 版和电脑版一样从服务器问新版本（`Updater`，平台 `android-arm64`，发布见 [管理后台](admin.md)
的"发布新版本"）：打开 10 秒后问一次，之后每 6 小时；有了在概览最上面出现"有新版本"，点"更新"下载到程序自己的目录，
核对签名、大小、sha256 以后经 Qt 的 FileProvider（`AndroidManifest.xml` 的 `qtprovider`）交给系统的安装界面。
第一次要在系统里允许比邻"安装未知应用"（`REQUEST_INSTALL_PACKAGES`）。装上了系统会结束程序；在安装界面取消了，
回到程序时又是"有新版本"，再点直接装刚才下载好的。设置 › 关于比邻里也能检查更新。

## 数据

程序数据在程序自己的目录里（`rc::stateDirectory()`），设备令牌和私钥原样存（只有本程序能读，`user_seal_plain.cpp`）。
Android 的设备名是"设置 › 关于手机"里的名字，硬件编号是 ANDROID_ID（重装以后登录同一个账号接回原来的设备 ID）。
