#!/bin/sh
# 从 Logo（App/branding/logo.svg）生成程序图标，生成的文件也放进仓库（编译时不用这些工具）：
# - App/branding/Bilin.icns：macOS，按苹果的图标规格，方块占 824/1024，四周留白
# - App/branding/Bilin.ico：Windows，16~256 几种大小，方块铺满
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

render() {
    "$qt/bin/qml" -platform offscreen "$work/render.qml" -- "$logo" "$1" "$2"
}

# Windows：铺满
mkdir -p "$work/ico"
render "$work/ico" 16,20,24,32,40,48,64,256
magick "$work/ico/16.png" "$work/ico/20.png" "$work/ico/24.png" "$work/ico/32.png" "$work/ico/40.png" \
    "$work/ico/48.png" "$work/ico/64.png" "$work/ico/256.png" "$root/App/branding/Bilin.ico"

# macOS：方块占 824/1024，四周留白（各个大小都按这个比例）
mkdir -p "$work/mac" "$work/Bilin.iconset"
render "$work/mac" 13,26,52,105,206,412,824
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

ls -la "$root/App/branding/Bilin.icns" "$root/App/branding/Bilin.ico"
