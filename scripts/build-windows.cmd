@echo off
chcp 65001 >nul
rem Windows 上配置并编译（MSVC + Ninja，RelWithDebInfo），给了安装目录就再装成能直接运行的程序：
rem
rem   scripts\build-windows.cmd <Qt 目录> [构建目录] [安装目录]
rem   scripts\build-windows.cmd C:\Qt\6.11.2\msvc2022_64 build\windows D:\Bilin
rem
rem 编哪种架构看 Qt 目录：msvc2022_arm64 编 ARM64，msvc2022_64 编 x64。ARM64 的电脑上也能编 x64（发布的 Windows
rem 安装包是 x64 的）：用 VS 的交叉编译器，Qt 的 x64 工具在 ARM64 的 Windows 上照样能跑。CMake 和 Ninja 用 Qt 安装里带的。
rem 安装目录里是程序和它用到的 Qt 库、插件、QML 模块，双击 Bilin.exe 运行。
rem 环境变量 RC_CMAKE_ARGS 里的也交给 CMake（安装包用它连正式的服务器）：每次都带着它重新配置，构建目录里记着的旧地址会被换掉。
setlocal
if "%~1"=="" goto usage
set "QT_DIR=%~f1"
set "SOURCE=%~dp0.."
set "BUILD=%~2"
if "%BUILD%"=="" set "BUILD=%SOURCE%\build\windows"
set "PREFIX=%~3"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VS="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VS=%%i"
if not defined VS goto novs

set "VCVARS=vcvars64.bat"
set "TARGET_ARGS="
if /i "%PROCESSOR_ARCHITECTURE%"=="ARM64" set "VCVARS=vcvarsarm64.bat"
rem ARM64 上编 x64：告诉 CMake 目标是 x64（不然它按本机当成 ARM64，SDK 带的 Opus 会打开 ARM 的指令）；
rem 编出来的 x64 程序这里能直接运行，所以不算交叉编译，Qt 照常用它自己的 x64 工具
if /i "%PROCESSOR_ARCHITECTURE%"=="ARM64" echo "%QT_DIR%" | findstr /i /r /c:"msvc20[0-9][0-9]_64" >nul && (
    set "VCVARS=vcvarsarm64_amd64.bat"
    set "TARGET_ARGS=-DCMAKE_SYSTEM_NAME=Windows -DCMAKE_SYSTEM_VERSION=10.0 -DCMAKE_SYSTEM_PROCESSOR=AMD64 -DCMAKE_CROSSCOMPILING=OFF"
)
call "%VS%\VC\Auxiliary\Build\%VCVARS%" >nul || exit /b 1

for %%d in ("%QT_DIR%\..\..") do set "QT_ROOT=%%~fd"
set "PATH=%QT_ROOT%\Tools\CMake_64\bin;%QT_ROOT%\Tools\CMake\bin;%QT_ROOT%\Tools\Ninja;%PATH%"

cmake -S "%SOURCE%" -B "%BUILD%" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo "-DCMAKE_PREFIX_PATH=%QT_DIR%" %TARGET_ARGS% %RC_CMAKE_ARGS% || exit /b 1
cmake --build "%BUILD%" || exit /b 1
if "%PREFIX%"=="" exit /b 0
cmake --install "%BUILD%" --component app --prefix "%PREFIX%" || exit /b 1
exit /b 0

:usage
echo 用法: %~nx0 ^<Qt 目录^> [构建目录] [安装目录]
exit /b 2

:novs
echo 没有找到 Visual Studio（vswhere.exe）
exit /b 1
