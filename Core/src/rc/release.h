#pragma once

#include <QByteArray>
#include <QString>

// 程序的新版本（docs/admin.md 的"发布新版本"）：管理后台发布的安装包由作者的发布密钥签名，程序下载以后
// 自己核对，服务器被人控制了也推不了假的安装包。签的内容和 Server/src/releases.ts、scripts/sign-release.mjs
// 一字不差
namespace rc::release {

// 这台电脑要的安装包：macos-arm64 | windows-x64；别的（还没有安装包）为空
QString platform();

// 签名签的那几行
QByteArray text(const QString &platform, const QString &version, qint64 size, const QByteArray &sha256Hex);

// 程序里带着的发布公钥（Server/release-key.pub，CMake 读进来）：P-256 未压缩的点，65 字节
QByteArray publicKey();

// signature 是 P-256 ECDSA（SHA-256）的 r||s，64 字节。各平台用系统的加密库：release_mac.cpp、release_win.cpp
bool signatureValid(const QByteArray &publicKey, const QByteArray &text, const QByteArray &signature);

} // namespace rc::release
