#pragma once

// Windows 安装程序（Bilin-Setup.exe）里接在程序后面的文件：每个文件压缩过（系统的 LZMS）的内容，
// 然后是目录表（每项一个 Entry，后面跟着 UTF-8 的相对路径），最后是 Footer。
// pack.cpp 写它（写完用 read 读回来核对一遍），stub.cpp 用 read 解压
#include <windows.h>

#include <compressapi.h>

#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

namespace archive {

inline constexpr char Magic[8] = {'R', 'C', 'S', 'E', 'T', 'U', 'P', '1'};

struct Footer
{
    std::uint64_t tableOffset;
    std::uint32_t files;
    char magic[8];
};

struct Entry
{
    std::uint64_t offset; // 压缩后的内容在整个文件里从哪开始
    std::uint64_t compressedSize;
    std::uint64_t size; // 0：空文件，没有内容
    std::uint32_t pathBytes;
};

// data 是整个安装程序：把后面接着的文件一个一个解压，交给 each(相对路径（UTF-8）, 内容)，each 返回 false 就停。
// 不是这种格式、内容不全（没下载完）、解压不了时返回 false
template<typename Each>
bool read(const std::vector<char> &data, Each &&each)
{
    Footer footer;
    if (data.size() < sizeof footer)
        return false;
    std::memcpy(&footer, data.data() + data.size() - sizeof footer, sizeof footer);
    const std::uint64_t tableEnd = data.size() - sizeof footer;
    if (std::memcmp(footer.magic, Magic, sizeof Magic) != 0 || footer.tableOffset > tableEnd)
        return false;
    DECOMPRESSOR_HANDLE decompressor = nullptr;
    if (!CreateDecompressor(COMPRESS_ALGORITHM_LZMS, nullptr, &decompressor))
        return false;
    std::uint64_t at = footer.tableOffset;
    bool ok = true;
    for (std::uint32_t i = 0; ok && i < footer.files; ++i) {
        Entry entry;
        ok = tableEnd - at >= sizeof entry;
        if (!ok)
            break;
        std::memcpy(&entry, data.data() + at, sizeof entry);
        at += sizeof entry;
        ok = tableEnd - at >= entry.pathBytes && entry.offset <= footer.tableOffset
             && footer.tableOffset - entry.offset >= entry.compressedSize;
        if (!ok)
            break;
        const std::string_view path(data.data() + at, entry.pathBytes);
        at += entry.pathBytes;
        std::vector<char> content(entry.size);
        if (entry.size > 0) {
            SIZE_T size = 0;
            ok = Decompress(decompressor, data.data() + entry.offset, SIZE_T(entry.compressedSize), content.data(),
                            content.size(), &size)
                 && size == entry.size;
        }
        ok = ok && each(path, content);
    }
    CloseDecompressor(decompressor);
    return ok && at == tableEnd;
}

} // namespace archive
