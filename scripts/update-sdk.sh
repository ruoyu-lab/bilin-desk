#!/bin/sh
# 把 SDK（子模块 third_party/rcsdk，https://github.com/ruoyu-lab/remote-control-sdk）换成另一个版本：
#
#   scripts/update-sdk.sh v0.1.1-beta        标签、提交都行，要是已经推到 GitHub 上的
#
# SDK 1.0 之前每个版本都可能有破坏性修改：换好以后按它的 CHANGELOG.md 同步修改程序，和子模块的改动一起提交
set -eu

ref="$1"
sdk="$(cd "$(dirname "$0")/.." && pwd)/third_party/rcsdk"

git -C "$sdk" fetch --quiet --tags origin
git -C "$sdk" checkout --quiet --detach "$ref"
version="$(awk '
    $1 == "#define" && $2 ~ /^RCSDK_VERSION_(MAJOR|MINOR|PATCH)$/ { sub(/u$/, "", $3); v[$2] = $3 }
    $1 == "#define" && $2 == "RCSDK_VERSION_LABEL" { gsub(/"/, "", $3); label = $3 }
    END { print v["RCSDK_VERSION_MAJOR"] "." v["RCSDK_VERSION_MINOR"] "." v["RCSDK_VERSION_PATCH"] (label == "" ? "" : "-" label) }' \
    "$sdk/include/rcsdk/rcsdk.h")"
echo "third_party/rcsdk <- $version ($(git -C "$sdk" rev-parse HEAD))"
