#include "phone_window.h"

#include "phone_system.h"

void PhoneWindow::setKeepAwake(bool keep)
{
    if (keep == keepAwake_)
        return;
    keepAwake_ = keep;
    phone_system::keepScreenOn(keep);
    emit keepAwakeChanged();
}

void PhoneWindow::setDark(bool dark)
{
    if (dark_ == dark)
        return;
    dark_ = dark;
    phone_system::setDark(dark);
    emit darkChanged();
}

void PhoneWindow::pressFeedback()
{
    phone_system::pressFeedback();
}

void PhoneWindow::hideHomeIndicator()
{
    phone_system::hideHomeIndicator();
}
