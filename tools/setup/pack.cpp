// 做 Windows 的安装程序（scripts\build-windows-installer.cmd 调它）：
//   BilinPack.exe <安装程序 BilinSetup.exe> <程序目录> <输出的 Bilin-Setup.exe>
// 输出 = 安装程序本身 + 程序目录里的每个文件（压缩过）+ 目录表（archive.h）。写完读回来，和程序目录里的逐个核对
#include "archive.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::vector<char> readFile(const fs::path &path)
{
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::string utf8(const std::wstring &text)
{
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), int(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(bytes, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), int(text.size()), result.data(), bytes, nullptr, nullptr);
    return result;
}

std::wstring wide(std::string_view text)
{
    const int chars = MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0);
    std::wstring result(chars, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), result.data(), chars);
    return result;
}

// 安装程序解压出来的（同一个 archive::read）和程序目录里的一样
bool verify(const fs::path &output, const fs::path &directory, std::size_t files)
{
    std::size_t checked = 0;
    const bool same = archive::read(readFile(output), [&](std::string_view name, const std::vector<char> &content) {
        ++checked;
        if (content == readFile(directory / wide(name)))
            return true;
        std::fprintf(stderr, "%ls 解压出来和原来的不一样\n", wide(name).c_str());
        return false;
    });
    return same && checked == files;
}

} // namespace

int wmain(int count, wchar_t **arguments)
{
    if (count != 4) {
        std::fprintf(stderr, "用法：BilinPack <安装程序> <程序目录> <输出>\n");
        return 2;
    }
    const fs::path stub = arguments[1];
    const fs::path directory = arguments[2];
    const fs::path output = arguments[3];
    COMPRESSOR_HANDLE compressor = nullptr;
    if (!CreateCompressor(COMPRESS_ALGORITHM_LZMS, nullptr, &compressor)) {
        std::fprintf(stderr, "建不了压缩器：%lu\n", GetLastError());
        return 1;
    }
    std::ofstream out(output, std::ios::binary | std::ios::trunc);
    const std::vector<char> self = readFile(stub);
    out.write(self.data(), std::streamsize(self.size()));

    std::vector<std::pair<archive::Entry, std::string>> table;
    for (const fs::directory_entry &file : fs::recursive_directory_iterator(directory)) {
        if (!file.is_regular_file())
            continue;
        const std::vector<char> content = readFile(file.path());
        archive::Entry entry{std::uint64_t(out.tellp()), 0, content.size(), 0};
        if (!content.empty()) {
            SIZE_T needed = 0;
            Compress(compressor, content.data(), content.size(), nullptr, 0, &needed);
            std::vector<char> packed(needed);
            SIZE_T packedSize = 0;
            if (!Compress(compressor, content.data(), content.size(), packed.data(), packed.size(), &packedSize)) {
                std::fprintf(stderr, "压缩不了 %ls：%lu\n", file.path().c_str(), GetLastError());
                return 1;
            }
            entry.compressedSize = packedSize;
            out.write(packed.data(), std::streamsize(packedSize));
        }
        const std::string path = utf8(fs::relative(file.path(), directory).wstring());
        entry.pathBytes = std::uint32_t(path.size());
        table.push_back({entry, path});
    }
    CloseCompressor(compressor);

    archive::Footer footer{std::uint64_t(out.tellp()), std::uint32_t(table.size()), {}};
    std::memcpy(footer.magic, archive::Magic, sizeof archive::Magic);
    for (const auto &[entry, path] : table) {
        out.write(reinterpret_cast<const char *>(&entry), sizeof entry);
        out.write(path.data(), std::streamsize(path.size()));
    }
    out.write(reinterpret_cast<const char *>(&footer), sizeof footer);
    out.close();
    if (!out.good()) {
        std::fprintf(stderr, "写不了 %ls\n", output.c_str());
        return 1;
    }
    if (!verify(output, directory, table.size())) {
        std::fprintf(stderr, "核对 %ls 没通过\n", output.c_str());
        return 1;
    }
    std::printf("%zu 个文件，核对过了\n", table.size());
    return 0;
}
