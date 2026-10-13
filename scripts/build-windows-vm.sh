#!/bin/zsh
# 在 Mac 上做 Windows x64 的安装包 Bilin-Setup.exe：用 Parallels 里的 Windows（ARM64 也行，交叉编译成 x64）。
# 把已经提交的源码（git archive HEAD 和它指着的 SDK，没提交的改动不在里面）经共享的"下载"文件夹交给虚拟机，虚拟机里跑
# scripts/build-windows-vm.ps1（编译、做安装包、静默装一遍试试），编成了才拷回 build/release/<版本>/，
# 再核对一遍两边的 SHA-256。之前那次的安装包先删掉：没编成就没有安装包可拿，不会拿到旧的
#
#   scripts/build-windows-vm.sh
#
# 虚拟机里要装好 Visual Studio（C++，ARM64 上还要"ARM64 上的 x64 编译器"）和 x64 的 Qt（msvc2022_64）。
#   BILIN_VM      虚拟机的名字（prlctl list），默认 "Windows 11"
#   BILIN_VM_QT   虚拟机里 x64 的 Qt，默认 C:\Qt\6.11.2\msvc2022_64
#   BILIN_COPY_TO 编好后再拷一份到别的 Windows（scp 的目标，比如 me@pc:C:/Users/me/Desktop/installer），核对哈希
set -eu

vm="${BILIN_VM:-Windows 11}"
qt="${BILIN_VM_QT:-C:\\Qt\\6.11.2\\msvc2022_64}"
root="${0:A:h:h}"
# 版本带着阶段（0.1.0-beta），同 CMakeLists.txt 的 BILIN_VERSION
version=$(sed -n 's/^project(Bilin VERSION \([0-9.]*\).*/\1/p' "$root/CMakeLists.txt")
label=$(sed -n 's/^set(BILIN_VERSION_LABEL \([a-z]*\)).*/\1/p' "$root/CMakeLists.txt")
[[ -n $label ]] && version="$version-$label"
shared="$HOME/Downloads"

# 在虚拟机里跑一段 PowerShell，打出来的东西原样返回（去掉 PowerShell 的进度记录）
run_in_vm() {
    local encoded=$(printf '%s' "$1" | iconv -f UTF-8 -t UTF-16LE | base64)
    prlctl exec "$vm" powershell -NoProfile -EncodedCommand "$encoded" 2>&1 | grep -v '^#< CLIXML' | sed 's/<Objs.*//'
}

# 子模块（SDK）不在 git archive 里：它指着的那个提交单独打包，接在后面
git -C "$root" archive --format=tar -o "$shared/bilin-src.tar" HEAD
git -C "$root/third_party/rcsdk" archive --format=tar --prefix=third_party/rcsdk/ -o "$shared/bilin-sdk.tar" \
    "$(git -C "$root" rev-parse HEAD:third_party/rcsdk)"
tar -rf "$shared/bilin-src.tar" "@$shared/bilin-sdk.tar"
rm -f "$shared/bilin-sdk.tar"
cp "$root/scripts/build-windows-vm.ps1" "$shared/bilin-build.ps1"
trap 'rm -f "$shared/bilin-src.tar" "$shared/bilin-sdk.tar" "$shared/bilin-build.ps1"' EXIT
output=$(run_in_vm "Copy-Item \\\\Mac\\Home\\Downloads\\bilin-build.ps1 C:\\src\\bilin-build.ps1 -Force; powershell -NoProfile -ExecutionPolicy Bypass -File C:\\src\\bilin-build.ps1 -Qt '$qt'")
print -r -- "$output"
vm_hash=$(print -r -- "$output" | sed -n 's/^BUILD-OK \([0-9a-f]*\).*/\1/p')
if [[ -z $vm_hash ]]; then
    print "没编成，不拷"
    exit 1
fi

run_in_vm 'Copy-Item C:\src\bilin-out\Bilin-Setup.exe \\Mac\Home\Downloads\bilin-setup.exe -Force' >/dev/null
out="$root/build/release/$version"
mkdir -p "$out"
# 虚拟机经共享文件夹写的文件属于 root：复制一份成自己的，再删掉它
cp "$shared/bilin-setup.exe" "$out/Bilin-Setup.exe"
rm -f "$shared/bilin-setup.exe"
if [[ $(shasum -a 256 "$out/Bilin-Setup.exe" | cut -d' ' -f1) != $vm_hash ]]; then
    print "拷到 Mac 的和虚拟机里的不一样"
    exit 1
fi
print "$out/Bilin-Setup.exe（$vm_hash）"

if [[ -n ${BILIN_COPY_TO:-} ]]; then
    target="${BILIN_COPY_TO%/}/Bilin-Setup-$version.exe"
    scp -q -o BatchMode=yes "$out/Bilin-Setup.exe" "$target"
    host=${target%%:*}
    file=${${target#*:}//\//\\}
    copied=$(ssh -o BatchMode=yes "$host" "certutil -hashfile \"$file\" SHA256" | sed -n 2p | tr -d '\r ')
    if [[ $copied != $vm_hash ]]; then
        print "拷到 $host 的不一样"
        exit 1
    fi
    print "也拷到了 $target"
fi
