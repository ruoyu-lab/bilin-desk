#pragma once

// 别人连本机的四个分享会话（Sharing：别人的、我的设备的、网页版的我的设备的、信任的设备的）各有各的编号
// （连着本机的人是 SDK 的 viewer_id，互传的文件是 rc::TransferList 的），给界面的合成一个：除以 4 的余数是哪个会话
namespace share_ids {

enum class Session { Others = 0, Mine = 1, WebMine = 2, Trusted = 3 };
constexpr int Sessions = 4;

constexpr int global(Session session, int local)
{
    return local * Sessions + int(session);
}

constexpr Session session(int global)
{
    return Session(global % Sessions);
}

constexpr int local(int global)
{
    return global / Sessions;
}

} // namespace share_ids
