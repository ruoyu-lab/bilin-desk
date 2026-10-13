#pragma once

#include <QString>

namespace rc {

// 临时密码：6 位小写字母和数字，去掉容易看错、念错的 0 o 1 l i
QString generateTemporaryPassword();

// 临时密码里可能出现的字符
inline constexpr char TemporaryPasswordAlphabet[] = "abcdefghjkmnpqrstuvwxyz23456789";
inline constexpr qsizetype TemporaryPasswordLength = 6;

} // namespace rc
