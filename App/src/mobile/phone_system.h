#pragma once

// 手机系统那边的几样（PhoneWindow 用）：phone_system_android.cpp、phone_system_ios.mm；
// 电脑上预览手机的界面时什么都不做（phone_system_none.cpp）
namespace phone_system {

// 屏幕不自己熄灭
void keepScreenOn(bool keep);
// 系统栏（状态栏、Android 的导航条）跟着程序的外观：深色时图标是白的
void setDark(bool dark);
// 手机震一下（按住不放起了作用）
void pressFeedback();
// 全屏的平板（iPad）：底部的横条不碰时自己隐去（苹果不让彻底去掉），从底边往上划先给程序，再划一下才回桌面。
// Android 上窗口全屏（Window.FullScreen）时状态栏、导航条已经藏起来了，这里什么都不做
void hideHomeIndicator();

} // namespace phone_system
