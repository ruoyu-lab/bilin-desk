#!/bin/zsh
# 编 iOS 版（手机版，App/CMakeLists.txt 的 BILIN_MOBILE），用 Xcode 里登录的 Apple ID 签名；带 --install 时装到
# USB 连着的 iPad、iPhone 上并打开。连的是正式的服务器 wss://www.bilindesk.com/ws（和安装包一样）
#
#   scripts/build-ios.sh [--install]
#
# 要：Xcode（设置 › 帐户里登录了 Apple ID）、Qt 的 iOS 版（~/Qt/6.11.2/ios）。设备第一次装之前：打开"设置 › 隐私与
# 安全性 › 开发者模式"，装上以后在"设置 › 通用 › VPN 与设备管理"里信任自己的开发者证书。
#   QT_ROOT              Qt 的目录，默认 ~/Qt/6.11.2
#   BILIN_IOS_BUNDLE_ID  程序的标识，默认 com.bilindesk.bilin.dev：个人团队（免费的，装上 7 天有效）签的用它，
#                        正式的 com.bilindesk.bilin 留给开发者账号
#   BILIN_IOS_DEVICE     装到哪台（xcrun devicectl list devices 里的名字或标识），默认连着的第一台
set -eu

root="${0:A:h:h}"
qt="${QT_ROOT:-$HOME/Qt/6.11.2}"
build="$root/build/ios"
bundle="${BILIN_IOS_BUNDLE_ID:-com.bilindesk.bilin.dev}"

if [[ ! -f "$build/CMakeCache.txt" ]]; then
    "$qt/ios/bin/qt-cmake" -S "$root" -B "$build" -G Xcode \
        -DQT_HOST_PATH="$qt/macos" \
        -DCMAKE_OSX_SYSROOT=iphoneos \
        -DBUILD_TESTING=OFF \
        -DRC_SERVER_URL=wss://www.bilindesk.com/ws \
        -DBILIN_IOS_BUNDLE_ID="$bundle"
fi
# 签名要的证书、描述文件由 Xcode 按登录的账号自己建（-allowProvisioningUpdates）
xcodebuild -project "$build/Bilin.xcodeproj" -scheme Bilin -configuration Release -destination generic/platform=iOS \
    -allowProvisioningUpdates -quiet build
app="$build/App/Release-iphoneos/Bilin.app"
echo "$app"

if [[ "${1:-}" == "--install" ]]; then
    device="${BILIN_IOS_DEVICE:-$(xcrun devicectl list devices --hide-headers 2>/dev/null \
        | awk '/available/ { for (i = 1; i <= NF; i++) if ($i ~ /^[0-9A-F]{8}-/) { print $i; exit } }')}"
    xcrun devicectl device install app --device "$device" "$app"
    xcrun devicectl device process launch --device "$device" "$bundle"
fi
