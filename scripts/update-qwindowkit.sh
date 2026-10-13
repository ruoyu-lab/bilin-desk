#!/bin/sh
# 用 QWindowKit 仓库的某个提交（含各级子模块：qmsetup 和它的 stdcorelib 等）替换 third_party/qwindowkit。
#
#   scripts/update-qwindowkit.sh <提交或标签>
set -eu

ref="${1:?用法: $0 <提交或标签>}"
root="$(cd "$(dirname "$0")/.." && pwd)"
dest="$root/third_party/qwindowkit"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

git clone --quiet https://github.com/stdware/qwindowkit.git "$work/src"
git -C "$work/src" checkout --quiet "$ref"
git -C "$work/src" submodule update --quiet --init --recursive
commit="$(git -C "$work/src" rev-parse HEAD)"

rm -rf "$dest"
mkdir -p "$dest"
git -C "$work/src" archive --format=tar "$commit" | tar -x -C "$dest"
# 每一级子模块按它被引用的提交放到对应的路径
submodules="$(git -C "$work/src" submodule foreach --quiet --recursive 'echo "$displaypath $sha1"')"
printf '%s\n' "$submodules" | while read -r path sha; do
    mkdir -p "$dest/$path"
    git -C "$work/src/$path" archive --format=tar "$sha" | tar -x -C "$dest/$path"
done

{
    echo "commit=$commit"
    echo "date=$(git -C "$work/src" show -s --format=%cI "$commit")"
    printf '%s\n' "$submodules" | sed 's/^\([^ ]*\) /submodule \1=/'
} > "$root/third_party/qwindowkit.snapshot"

echo "third_party/qwindowkit <- $commit"
printf '%s\n' "$submodules"
