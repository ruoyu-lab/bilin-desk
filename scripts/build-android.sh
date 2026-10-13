#!/bin/zsh
# 编 Android 版的 APK（手机版，App/CMakeLists.txt 的 BILIN_MOBILE），用调试密钥签名；带 --install 时装到连着的手机上
# 并打开。连的是正式的服务器 wss://www.bilindesk.com/ws（和安装包一样）
#
#   scripts/build-android.sh [--install]
#
# 要：Qt 的 Android 版（~/Qt/6.11.2/android_arm64_v8a）、Android SDK 和 NDK、JDK 17 起（Gradle 用）。
#   QT_ROOT        Qt 的目录，默认 ~/Qt/6.11.2
#   ANDROID_HOME   Android SDK，默认 ~/Library/Android/sdk（NDK 用它下面最新的一个）
#   JAVA_HOME      JDK，默认 Android Studio 自带的那个
set -eu

root="${0:A:h:h}"
qt="${QT_ROOT:-$HOME/Qt/6.11.2}"
sdk="${ANDROID_HOME:-$HOME/Library/Android/sdk}"
ndk="$(ls -d "$sdk"/ndk/* | sort -V | tail -1)"
export JAVA_HOME="${JAVA_HOME:-$(/usr/libexec/java_home -v 21 2>/dev/null || /usr/libexec/java_home)}"
build="$root/build/android"
tools="$(ls -d "$sdk"/build-tools/* | sort -V | tail -1)"

if [[ ! -f "$build/CMakeCache.txt" ]]; then
    # SDK 要 Android 10（API 29）起
    "$qt/android_arm64_v8a/bin/qt-cmake" -S "$root" -B "$build" -G Ninja \
        -DQT_HOST_PATH="$qt/macos" \
        -DANDROID_SDK_ROOT="$sdk" \
        -DANDROID_NDK_ROOT="$ndk" \
        -DANDROID_PLATFORM=android-29 \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTING=OFF \
        -DRC_SERVER_URL=wss://www.bilindesk.com/ws
fi
cmake --build "$build" --target apk

unsigned="$build/App/android-build/build/outputs/apk/release/android-build-release-unsigned.apk"
apk="$build/Bilin.apk"
"$tools/apksigner" sign --ks "$HOME/.android/debug.keystore" --ks-pass pass:android --key-pass pass:android \
    --out "$apk" "$unsigned"
echo "$apk"

if [[ "${1:-}" == "--install" ]]; then
    adb="$sdk/platform-tools/adb"
    "$adb" install -r "$apk"
    "$adb" shell am start -n com.bilindesk.bilin/org.qtproject.qt.android.bindings.QtActivity
fi
