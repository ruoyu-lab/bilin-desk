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
# 程序编好、放进打包目录（Bilin_prepare_apk_dir），打包自己来（下面）

# 打包：Qt 按 QML 用到的模块决定带上哪些库，可它总把整个 App 目录也拿去扫（那里还有电脑版、测试的 QML，
# 会带进托盘、测试、各种界面风格这些用不到的，APK 多出二十多 MB），CMake 里关不掉。所以用 Qt 生成的打包设置，
# 去掉 App 目录（手机版的 QML 所在的那几个目录留着），自己调打包工具（和 apk 目标的做法一样）
app="$build/App"
python3 - "$app/android-Bilin-deployment-settings.json" "$root/App" > "$app/android-Bilin-deployment-mobile.json" <<'PY'
import json, sys
settings = json.load(open(sys.argv[1]))
settings["qml-root-path"] = [p for p in settings["qml-root-path"] if p != sys.argv[2]]
json.dump(settings, sys.stdout, indent=1, ensure_ascii=False)
PY
# 上次打包放进去的 Qt 的库不会自己去掉：清掉再放程序进去
rm -rf "$app/android-build/libs"
cmake --build "$build" --target Bilin_prepare_apk_dir
"$qt/macos/bin/androiddeployqt" --input "$app/android-Bilin-deployment-mobile.json" --output "$app/android-build" \
    --apk "$app/android-build/Bilin.apk" --builddir "$app" --release

unsigned="$app/android-build/build/outputs/apk/release/android-build-release-unsigned.apk"
apk="$build/Bilin.apk"
"$tools/apksigner" sign --ks "$HOME/.android/debug.keystore" --ks-pass pass:android --key-pass pass:android \
    --out "$apk" "$unsigned"
echo "$apk"

if [[ "${1:-}" == "--install" ]]; then
    adb="$sdk/platform-tools/adb"
    "$adb" install -r "$apk"
    "$adb" shell am start -n com.bilindesk.bilin/org.qtproject.qt.android.bindings.QtActivity
fi
