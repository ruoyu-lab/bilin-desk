# 第三方代码

## rcsdk —— 远程控制 SDK

`rcsdk/` 是[远程控制 SDK](https://github.com/ruoyu-lab/remote-control-sdk) 的 git 子模块：屏幕和声音采集、编解码、加密传输、码率控制、远程输入等。以 `add_subdirectory` 编进程序（静态链接 `RemoteControl::Core`）。克隆时带上 `--recursive`，已经克隆了的跑 `git submodule update --init`。

- 作者：梨茶（与本程序同一作者）
- 许可：MIT（`rcsdk/LICENSE`）；SDK 自带的第三方代码的许可见 `rcsdk/THIRD_PARTY_NOTICES.md`
- 版本：子模块指着的那个标签（`git -C third_party/rcsdk describe --tags`），版本号在它的 `include/rcsdk/rcsdk.h` 里
- 更新：`scripts/update-sdk.sh <标签>`

SDK 在 1.0 之前每个版本都可能有破坏性修改：更新时按它的 `CHANGELOG.md` 同步修改本程序，和子模块的改动在同一个提交里完成。SDK 的问题回 SDK 仓库里修，不在子模块里改。

## qwindowkit —— Windows 的窗口外框

`qwindowkit/` 是 [QWindowKit](https://github.com/stdware/qwindowkit) 的源码副本，只在 Windows 上编译（静态库，只用 Qt Quick 模块）。它让程序去掉系统标题栏、自己画标题栏和最小化 / 最大化 / 关闭，同时保留系统的边框、阴影、圆角和 Windows 11 的贴靠布局。程序经 `App/src/windowchrome.cpp` 使用它，QML 不直接引用。

- 许可：QWindowKit 为 Apache License 2.0（`qwindowkit/LICENSE`）；它的构建依赖 qmsetup 和 stdcorelib 为 MIT（`qwindowkit/qmsetup/LICENSE`、`qwindowkit/qmsetup/src/stdcorelib/LICENSE`）
- 版本：见 `qwindowkit.snapshot`（提交和各级子模块的提交）
- 更新：`scripts/update-qwindowkit.sh <提交或标签>`

不在副本里修改。
