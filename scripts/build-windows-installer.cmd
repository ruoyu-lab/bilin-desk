@echo off
chcp 65001 >nul
rem 做 Windows 的安装包 Bilin-Setup.exe（连正式的服务器 www.bilindesk.com）：
rem
rem   scripts\build-windows-installer.cmd <Qt 目录> [构建目录] [输出目录]
rem   scripts\build-windows-installer.cmd C:\Qt\6.11.2\msvc2022_64 build\installer D:\Out
rem
rem 在构建目录里编（build-windows.cmd），装到构建目录的 stage，再把 stage 接到安装程序后面（tools\setup，编出来在构建目录的最上层）。
rem 输出目录默认是构建目录
setlocal
if "%~1"=="" goto usage
set "QT_DIR=%~f1"
set "SOURCE=%~dp0.."
set "BUILD=%~f2"
if "%~2"=="" set "BUILD=%SOURCE%\build\installer"
set "OUT=%~f3"
if "%~3"=="" set "OUT=%BUILD%"
set "STAGE=%BUILD%\stage"

set "RC_CMAKE_ARGS=-DRC_SERVER_URL=wss://www.bilindesk.com/ws"
if exist "%STAGE%" rmdir /s /q "%STAGE%"
call "%~dp0build-windows.cmd" "%QT_DIR%" "%BUILD%" "%STAGE%" || exit /b 1
if not exist "%OUT%" mkdir "%OUT%"
"%BUILD%\BilinPack.exe" "%BUILD%\BilinSetup.exe" "%STAGE%" "%OUT%\Bilin-Setup.exe" || exit /b 1
echo %OUT%\Bilin-Setup.exe
exit /b 0

:usage
echo 用法: %~nx0 ^<Qt 目录^> [构建目录] [输出目录]
exit /b 2
