#pragma once

// iPad、iPhone 接着的键盘（hardware_keys_ios.mm）：在 UIKit 把按键交给程序的第一步拦下来，要按键的画面
// （RemoteView::keyTarget）在控制对方时原样发过去，不经过 Qt。Qt 只认一部分键（删除、Del、回车……对不上），
// 打字时（输入框有焦点）还只给方向键；系统也不替接着的键盘连发按住的键，这里按住半秒以后自己连发
namespace hardware_keys {

// 第一个画面建好时装上，之后一直在
void install();

} // namespace hardware_keys
