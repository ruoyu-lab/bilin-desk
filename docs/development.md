# 开发

## 程序

需要 CMake 3.25+、Ninja、Qt 6.10+（Qt Quick、Quick Controls、Network、WebSockets、Shader Tools、SVG、Labs Platform）。
macOS 14+ 用 Xcode 的命令行工具；Windows 10+ 用 Visual Studio 2022 或更新版本（或 Build Tools，带"使用 C++ 的桌面开发"）。

macOS：

```sh
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=$HOME/Qt/6.11.2/macos
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
open build/debug/App/Bilin.app
```

Windows：第三个参数是安装目录，里面是程序和它用到的 Qt 库，双击 `Bilin.exe` 运行。ARM64 的电脑也能编 x64 版
（Qt 用 `msvc2022_64`，脚本会用 VS 的交叉编译器）：

```bat
scripts\build-windows.cmd C:\Qt\6.11.2\msvc2022_64 build\windows D:\Bilin
```

用 Qt Creator 时打开根目录的 `CMakeLists.txt`。

- 开发时编的程序连 Mac 上的开发服务器（下面），CMake 的 `RC_SERVER_URL` 默认 `ws://localhost:8800/ws`。
  这台电脑装了比邻时也一样：只有装好的那一份连系统服务，开发版连它旁边的后台服务（两份各有各的身份，
  局域网信令的端口被装好的占着时，开发版之间只能经服务器连）；
  局域网里别的电脑写 Mac 的地址，比如 `-DRC_SERVER_URL=ws://192.168.1.10:8800/ws`
- 日志在数据目录（macOS `~/Library/Application Support/Bilin`，Windows `%LOCALAPPDATA%\Bilin`）的 `app.log` 和 `service.log`
- 环境变量 `RC_STATE_DIR` 换掉数据目录（本机连接的名字也跟着换），测试时在同一台电脑上再跑一份完全隔开的
- macOS 上被别人连接要"屏幕与系统音频录制"和"辅助功能"两项授权，记在 `Bilin.app`（标识 `com.bilindesk.bilin`）名下。
  开发时每次编译都用临时签名，但签名的指定要求固定成这个标识，所以重新编译后授权还在，授权一次就够
- 测试（自动测试、端到端、只能人在电脑前测的）见 [testing.md](testing.md)

## 安装包

安装包连正式的服务器 `wss://www.bilindesk.com/ws`。

```sh
scripts/build-macos-pkg.sh build/production build/release/0.1.0-beta   # macOS：Bilin.pkg
scripts/build-windows-vm.sh                                          # Windows：在 Mac 上的 Parallels 虚拟机里编 Bilin-Setup.exe
```

在 Windows 上直接做（在 VS 的开发环境里编，VC 运行库跟着程序放）：

```bat
scripts\build-windows-installer.cmd C:\Qt\6.11.2\msvc2022_64 build\installer out
```

签名、发布新版本见 [admin.md](admin.md)。

## 服务端

Fastify + WebSocket，设备登记和账号存在 MongoDB，PM2 跑两个实例、经 Redis 互相转发，Nginx 在前面。
coturn 是直连用的 STUN 和直连打不通时的中继（端口 3478），服务器给设备发有时效的中继凭据（[network.md](network.md)）。

```sh
cd Server && npm install && npm test   # 测试不需要 MongoDB、Redis
Server/deploy/setup-dev.sh             # 在 Mac 上建开发用的服务器：OrbStack 的 Rocky 9 虚拟机 bilin-dev
Server/deploy/deploy-dev.sh            # 部署到它（Nginx 在 8800 端口，管理后台 http://localhost:8800/admin/）
```

正式的服务器（bilindesk.com）：

```sh
Server/deploy/setup-production.sh      # 第一次：装 Node、MongoDB、Redis、coturn、Nginx，申请证书；不动服务器上原来的东西
Server/deploy/deploy-production.sh     # 部署服务端、官网、管理后台
Server/deploy/set-admin-password.sh    # 设管理后台的密码
Server/deploy/harden-server.sh         # 安全加固（SSH、fail2ban、自动装安全更新）
```

管理后台见 [admin.md](admin.md)，官网见 [website.md](website.md)。

## SDK

画面、声音、连接、加密、远程输入都在 [远程控制 SDK](https://github.com/ruoyu-lab/remote-control-sdk)（MIT）里，
是 `third_party/rcsdk` 这个 git 子模块（克隆时带 `--recursive`，或者 `git submodule update --init`），以源码编进程序
（[third_party/README.md](../third_party/README.md)）。换版本：`scripts/update-sdk.sh <标签>`。
SDK 1.0 之前每个版本都可能有破坏性修改：更新时按它的 `CHANGELOG.md` 同步修改本程序，两端要用同一个版本的 SDK 才连得上。
