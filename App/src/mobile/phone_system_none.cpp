#include "phone_system.h"

#include <QString>

// 电脑上预览手机的界面：没有这些
namespace phone_system {

bool tablet()
{
    return qEnvironmentVariable("BILIN_PREVIEW") == u"tablet";
}

void keepScreenOn(bool)
{
}

void setDark(bool)
{
}

void pressFeedback()
{
}

void hideHomeIndicator()
{
}

void captureSystemKeys(bool)
{
}

} // namespace phone_system
