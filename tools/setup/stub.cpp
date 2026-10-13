// Windows 的安装程序 Bilin-Setup.exe 的程序部分：它后面接着压缩过的程序文件（archive.h，pack.cpp 接上的）。
// 双击：要管理员身份（setup.manifest），解压到临时目录，以安装模式启动里面的界面程序（--setup install），
// 等它退出以后删掉临时目录。安装界面、装些什么都在界面程序里（App/src/setup），这里只管解压。
// 不用现成的安装工具和 7-Zip 的自解压模块：都要从网上另外下载别人的程序
#include "archive.h"

#include <windows.h>

#include <shellapi.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

void fail(const wchar_t *message)
{
    MessageBoxW(nullptr, message, L"比邻", MB_OK | MB_ICONERROR);
}

fs::path ownPath()
{
    std::wstring path(32768, L'\0');
    path.resize(GetModuleFileNameW(nullptr, path.data(), DWORD(path.size())));
    return path;
}

std::vector<char> readFile(const fs::path &path)
{
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::wstring wide(const char *text, std::size_t bytes)
{
    const int chars = MultiByteToWideChar(CP_UTF8, 0, text, int(bytes), nullptr, 0);
    std::wstring result(chars, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text, int(bytes), result.data(), chars);
    return result;
}

// 删掉解压出来的：刚退出的程序文件可能还被杀毒软件扫着（占用），删不掉时等一会儿再试
void removeExtracted(const fs::path &directory)
{
    std::error_code error;
    for (int attempt = 0; attempt < 20; ++attempt) {
        fs::remove_all(directory, error);
        if (!fs::exists(directory, error))
            return;
        Sleep(250);
    }
}

bool extract(const std::vector<char> &self, const fs::path &into)
{
    return archive::read(self, [&into](std::string_view name, const std::vector<char> &content) {
        const fs::path path = into / wide(name.data(), name.size());
        std::error_code error;
        fs::create_directories(path.parent_path(), error);
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(content.data(), std::streamsize(content.size()));
        return out.good();
    });
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    wchar_t temp[MAX_PATH + 1];
    GetTempPathW(MAX_PATH + 1, temp);
    const fs::path directory = fs::path(temp) / (L"BilinSetup-" + std::to_wstring(GetCurrentProcessId()));
    std::error_code error;
    fs::remove_all(directory, error);
    if (!extract(readFile(ownPath()), directory)) {
        fail(L"安装程序坏了，重新下载一次再试。");
        fs::remove_all(directory, error);
        return 1;
    }
    // 安装程序的参数原样带过去（--quiet：不要界面）
    std::wstring command = L"\"" + (directory / L"Bilin.exe").wstring() + L"\" --setup install";
    bool quiet = false;
    int count = 0;
    if (wchar_t **arguments = CommandLineToArgvW(GetCommandLineW(), &count)) {
        for (int i = 1; i < count; ++i) {
            command += L" \"" + std::wstring(arguments[i]) + L"\"";
            quiet = quiet || std::wstring(arguments[i]) == L"--quiet";
        }
        LocalFree(arguments);
    }
    STARTUPINFOW startup{};
    startup.cb = sizeof startup;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr, directory.c_str(), &startup,
                        &process)) {
        fail(L"安装界面打不开。");
        fs::remove_all(directory, error);
        return 1;
    }
    // 装完（或者关掉安装界面）以后它退出
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    removeExtracted(directory);
    // 安装界面自己会说装没装成；它不是好好退出的（出了错没打开、崩了），这里说一声，不然像是没反应
    if (code != 0 && !quiet)
        fail(L"安装界面出了错，没能打开。\n日志在 %TEMP%\\BilinSetup.log，可以发给开发者看看。");
    return int(code);
}
