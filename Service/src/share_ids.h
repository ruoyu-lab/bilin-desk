#pragma once

// 别人连本机的三个分享会话（Sharing：别人的、我的设备的、网页版的我的设备的）各有各的编号（连着本机的人是 SDK 的
// viewer_id，互传的文件是 rc::TransferList 的），给界面的合成一个：除以 3 的余数是哪个会话
namespace share_ids {

enum class Session { Others = 0, Mine = 1, WebMine = 2 };

constexpr int global(Session session, int local)
{
    return local * 3 + int(session);
}

constexpr Session session(int global)
{
    return Session(global % 3);
}

constexpr int local(int global)
{
    return global / 3;
}

} // namespace share_ids
