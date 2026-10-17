#!/bin/sh
# 从 Logo（App/branding/logo.svg）生成程序图标，生成的文件也放进仓库（编译时不用这些工具）：
# - App/branding/Bilin.icns：macOS，按苹果的图标规格，方块占 824/1024，四周留白
# - App/branding/Bilin.ico：Windows，16~256 几种大小，方块铺满
# - App/platform/ios/Assets.xcassets/AppIcon.appiconset/AppIcon.png：iOS，1024 一张（Xcode 做出别的大小），铺满、
#   不透明、不要圆角和描边（系统自己切圆角）
# - Website/public：官网的 favicon-32.png、apple-touch-icon.png（180，同 iOS 的做法）和分享卡片的图 og.png（1200×630）
# 用 Qt 自己的 SVG 渲染出图（和程序里显示的一样），ImageMagick 拼 .ico，iconutil 拼 .icns。改了 Logo 再跑一遍
#
#   scripts/make-icons.sh
set -eu

root="$(cd "$(dirname "$0")/.." && pwd)"
qt="${QT_DIR:-$HOME/Qt/6.11.2/macos}"
logo="$root/App/branding/logo.svg"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# 一个 QML 小程序：把 Logo 按一串大小画成 PNG（方块铺满）
cat > "$work/render.qml" <<'QML'
import QtQuick
import QtQuick.Window

Window {
    id: w
    width: 1024
    height: 1024
    visible: true
    color: "transparent"
    readonly property var args: Qt.application.arguments
    readonly property var sizes: args[args.length - 1].split(",").map(Number)
    property int index: 0
    Image {
        id: image
        width: w.sizes[w.index]
        height: width
        sourceSize.width: width
        sourceSize.height: height
        source: "file://" + w.args[w.args.length - 3]
        onStatusChanged: if (status === Image.Ready) Qt.callLater(w.save)
    }
    function save() {
        image.grabToImage(result => {
            result.saveToFile(w.args[w.args.length - 2] + "/" + w.sizes[w.index] + ".png")
            if (w.index + 1 < w.sizes.length)
                w.index++
            else
                Qt.quit()
        })
    }
}
QML

# render <SVG> <输出目录> <大小,大小…>
render() {
    "$qt/bin/qml" -platform offscreen "$work/render.qml" -- "$1" "$2" "$3"
}

# Windows：铺满
mkdir -p "$work/ico"
render "$logo" "$work/ico" 16,20,24,32,40,48,64,256
magick "$work/ico/16.png" "$work/ico/20.png" "$work/ico/24.png" "$work/ico/32.png" "$work/ico/40.png" \
    "$work/ico/48.png" "$work/ico/64.png" "$work/ico/256.png" "$root/App/branding/Bilin.ico"

# macOS：方块占 824/1024，四周留白（各个大小都按这个比例）
mkdir -p "$work/mac" "$work/Bilin.iconset"
render "$logo" "$work/mac" 13,26,52,105,206,412,824
for pair in 16:13:icon_16x16 32:26:icon_16x16@2x 32:26:icon_32x32 64:52:icon_32x32@2x 128:105:icon_128x128 \
            256:206:icon_128x128@2x 256:206:icon_256x256 512:412:icon_256x256@2x 512:412:icon_512x512 \
            1024:824:icon_512x512@2x; do
    canvas=${pair%%:*}
    rest=${pair#*:}
    body=${rest%%:*}
    name=${rest#*:}
    magick "$work/mac/$body.png" -background none -gravity center -extent "${canvas}x${canvas}" \
        "$work/Bilin.iconset/$name.png"
done
iconutil -c icns -o "$root/App/branding/Bilin.icns" "$work/Bilin.iconset"

# iOS：去掉方块的圆角和描边，铺满，透明的地方填白
ios_icon="$root/App/platform/ios/Assets.xcassets/AppIcon.appiconset/AppIcon.png"
sed -e 's/ rx="186"//' -e '/stroke-opacity="0.08"/d' "$logo" > "$work/ios.svg"
mkdir -p "$work/ios"
render "$work/ios.svg" "$work/ios" 1024
magick "$work/ios/1024.png" -background white -alpha remove -alpha off "$ios_icon"

# 官网：favicon（带圆角，透明）、苹果设备加到主屏幕的图标（同 iOS）
web="$root/Website/public"
mkdir -p "$web" "$work/web"
render "$logo" "$work/web" 32
cp "$work/web/32.png" "$web/favicon-32.png"
render "$work/ios.svg" "$work/web" 180
magick "$work/web/180.png" -background white -alpha remove -alpha off "$web/apple-touch-icon.png"

# 官网分享卡片的图（og:image）：白底，左边 Logo，右边名字和一句话，和官网一个样子
cat > "$work/og.qml" <<'QML'
import QtQuick
import QtQuick.Window

Window {
    id: w
    width: 1200
    height: 630
    visible: true
    readonly property var args: Qt.application.arguments
    Rectangle {
        id: card
        width: 1200
        height: 630
        color: "#ffffff"
        Image {
            id: logo
            x: 150
            anchors.verticalCenter: parent.verticalCenter
            width: 280
            height: 280
            sourceSize.width: 280
            sourceSize.height: 280
            source: "file://" + w.args[w.args.length - 2]
            onStatusChanged: if (status === Image.Ready) Qt.callLater(w.save)
            // 本地的图可能在建界面的时候就读好了，那时上面那句还没接上
            Component.onCompleted: if (status === Image.Ready) Qt.callLater(w.save)
        }
        Column {
            anchors.left: logo.right
            anchors.leftMargin: 72
            anchors.verticalCenter: parent.verticalCenter
            spacing: 18
            Text { text: "比邻"; font.family: "PingFang SC"; font.pixelSize: 116; font.weight: Font.Black; color: "#16171a" }
            Text { text: "免费开源的远程桌面"; font.family: "PingFang SC"; font.pixelSize: 46; font.weight: Font.Medium; color: "#3a3a3c" }
            Text { text: "Windows · macOS · Android"; font.family: "PingFang SC"; font.pixelSize: 30; color: "#8e8e93" }
        }
        Text {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 40
            text: "bilindesk.com"
            font.family: "PingFang SC"
            font.pixelSize: 26
            color: "#aeaeb2"
        }
    }
    function save() {
        card.grabToImage(result => {
            result.saveToFile(w.args[w.args.length - 1])
            Qt.quit()
        })
    }
}
QML
"$qt/bin/qml" -platform offscreen "$work/og.qml" -- "$logo" "$work/og.png"
magick "$work/og.png" -background white -alpha remove -alpha off "$web/og.png"

ls -la "$root/App/branding/Bilin.icns" "$root/App/branding/Bilin.ico" "$ios_icon" "$web"
