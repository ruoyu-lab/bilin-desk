#!/bin/sh
# 做 macOS 的安装包 Bilin.pkg（连正式的服务器 www.bilindesk.com）：程序、开机就运行的后台服务
# （LaunchDaemon）、各个会话里的助手（LaunchAgent，rc/system_service.h）
#
#   scripts/build-macos-pkg.sh [构建目录] [输出目录]
#
# Qt 在 QT_DIR（默认 ~/Qt/6.11.2/macos）。装到"应用程序"里的 Bilin.app（卸载在程序的设置里）。
# 没有付费的开发者证书：程序用临时签名，安装包不签，第一次打开安装包要在"系统设置 › 隐私与安全性"里点"仍要打开"
set -eu

source="$(cd "$(dirname "$0")/.." && pwd)"
build="${1:-$source/build/package}"
out="${2:-$build}"
qt="${QT_DIR:-$HOME/Qt/6.11.2/macos}"
version=$(sed -n 's/^project(Bilin VERSION \([0-9.]*\).*/\1/p' "$source/CMakeLists.txt")
app_identifier=com.bilindesk.bilin
service_identifier=$app_identifier.service
agent_identifier=$app_identifier.agent

cmake -S "$source" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$qt" \
    -DRC_SERVER_URL=wss://www.bilindesk.com/ws
cmake --build "$build"

stage="$build/stage"
root="$build/pkgroot"
rm -rf "$stage" "$root"
cmake --install "$build" --component app --prefix "$stage"
mkdir -p "$root/Applications" "$root/Library/LaunchDaemons" "$root/Library/LaunchAgents"
ditto "$stage/Bilin.app" "$root/Applications/Bilin.app"
app="$root/Applications/Bilin.app"
cp "$source/App/platform/macos/launchd/$service_identifier.plist" "$root/Library/LaunchDaemons/"
cp "$source/App/platform/macos/launchd/$agent_identifier.plist" "$root/Library/LaunchAgents/"
chmod 644 "$root/Library/LaunchDaemons/"*.plist "$root/Library/LaunchAgents/"*.plist

# 部署工具改过包里的库和程序：重新签。它只顾 Contents/MacOS 里的程序，Contents/Helpers 里后台服务、助手用到的库
# （QtWebSockets 这些）它改了却没签：库、插件先都签一遍
find "$app/Contents/Frameworks" -maxdepth 1 -name '*.framework' -exec codesign --force --sign - {} \;
find "$app/Contents/PlugIns" -type f -name '*.dylib' -exec codesign --force --sign - {} +
# 程序：指定要求固定成标识（同开发时，App/CMakeLists.txt）：升级以后屏幕录制、辅助功能的授权还在
codesign --force --sign - --identifier "$service_identifier" "-r=designated => identifier \"$service_identifier\"" \
    "$app/Contents/Helpers/BilinService"
codesign --force --sign - --identifier "$agent_identifier" "-r=designated => identifier \"$agent_identifier\"" \
    "$app/Contents/Helpers/BilinAgent"
codesign --force --sign - --identifier "$app_identifier" "-r=designated => identifier \"$app_identifier\"" "$app"
codesign --verify --deep --strict "$app"

# 装到固定的位置：不让安装器去"升级"别处同一个标识的程序（比如构建目录里的开发版本）
pkgbuild --analyze --root "$root" "$build/component.plist"
plutil -replace 0.BundleIsRelocatable -bool NO "$build/component.plist"
mkdir -p "$out"
pkgbuild --root "$root" --component-plist "$build/component.plist" --identifier "$app_identifier" \
    --version "$version" --install-location / --scripts "$source/App/platform/macos/pkg" "$out/Bilin.pkg"
echo "$out/Bilin.pkg"
