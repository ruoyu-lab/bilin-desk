#!/bin/sh
# 编 SDK 的浏览器包（WebAssembly，docs/web.md），放进网页版 Web/public/sdk/（不进仓库）。
# 要 Emscripten SDK（EMSDK 环境变量，或者装在 ~/emsdk）。编在 build/rcsdk-wasm，SDK 的目录里不留东西
#
#   scripts/build-web-sdk.sh
set -eu

root="$(cd "$(dirname "$0")/.." && pwd)"
build="$root/build/rcsdk-wasm"
cmake -S "$root/third_party/rcsdk" --preset wasm -B "$build" -DBUILD_TESTING=OFF >/dev/null
cmake --build "$build" --target rcsdk_web
mkdir -p "$root/Web/public/sdk"
cp "$build/web/rcsdk.mjs" "$build/web/rcsdk_core.mjs" "$build/web/rcsdk_core.wasm" "$build/web/rcsdk_audio.js" \
    "$root/Web/public/sdk/"
echo "SDK 的浏览器包在 Web/public/sdk/"
