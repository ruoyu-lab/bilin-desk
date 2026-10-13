#pragma once

// 别人连本机的两个分享会话（Sharing：别人的、我的设备的）各有各的编号（连着本机的人是 SDK 的 viewer_id，
// 互传的文件是 rc::TransferList 的），给界面的合成一个：别人的是偶数，我的设备的是奇数
namespace share_ids {

enum class Session { Others = 0, Mine = 1 };

constexpr int global(Session session, int local)
{
    return local * 2 + int(session);
}

constexpr Session session(int global)
{
    return global % 2 == 1 ? Session::Mine : Session::Others;
}

constexpr int local(int global)
{
    return global / 2;
}

} // namespace share_ids
