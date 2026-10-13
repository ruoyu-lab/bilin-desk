#pragma once

#include <rcsdk/rcsdk.h>

#include <QList>

// 会话里选的分辨率、帧率、码率、编码格式。是观看端向分享端提的要求（rcs_view_request_config），分享端在它的上限
// （对方显示器的原始分辨率、刷新率）以内照办，运行中随时换、不用重连。对方的画面是连着它的所有观看端共用的，
// 由第一个连进去的（SDK 的主观看端）的要求决定。
// 选的是"长边多少像素"（0：原始分辨率）、"每秒多少帧"（0：对方显示器的刷新率）、"最多多少 Mbit/s"（0：自动，
// 跟着分辨率和帧率定）和编码格式（0：自动）：记在偏好设置里，连哪台都用，落到这一台上时不超过它的上限。
// 网络差时 SDK 再把码率往下调
namespace view_quality {

// 对方正在分享的显示器（rcs_stream_info 的 display_*）；还不知道时 width 为 0
struct Display
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t refreshHz = 0; // 0：系统没说，按 60 算
};

// 分辨率的选项：长边的像素，从大到小，第一个是 0（原始分辨率），后面只有比原始小的。还不知道显示器时为空
QList<uint32_t> longSides(const Display &display);
// 帧率的选项：从大到小，第一个是显示器的刷新率，后面只有比它小的
QList<uint32_t> frameRates(const Display &display);

// 选的长边、帧率落到这块显示器上，是选项里的第几个：比原始还大的、不在选项里的，取不超过它的那一个
int longSideIndex(uint32_t longSide, const Display &display);
int frameRateIndex(uint32_t frameRate, const Display &display);

// 码率的选项（Mbit/s），从小到大，第一个是 0（自动）
QList<uint32_t> bitrates();
// 选的码率是选项里的第几个：不在选项里的取不超过它的那一个
int bitrateIndex(uint32_t mbps);

// 一种两边都能用的编码格式；hardware：两边都用显卡（对方编、本机解），否则至少一边要用 CPU；
// clearText：两边都用显卡做得了 4:4:4（颜色精细到每个像素，字边不糊）
struct Codec
{
    rcs_codec codec = 0;
    bool hardware = false;
    bool clearText = false;

    friend bool operator==(const Codec &, const Codec &) = default;
};
// 编码格式的选项，按 HEVC、H.264、AV1、VP9 的顺序：对方提供的（rcs_peer_info.codecs）、本机解得了的（显卡、CPU），
// 都是 (1u << rcs_codec) 的位。两边都用显卡的有就只列它们；一个都没有时才列要用 CPU 的（万不得已）。
// clearText：两边都用显卡做得了 4:4:4 的（对方提供的里带着 4:4:4，本机的显卡解得了）
QList<Codec> codecs(uint32_t peerOffer, uint32_t peerHardware, uint32_t localHardware, uint32_t localSoftware,
                    uint32_t clearText);
// 做得了 4:4:4 的那几种
QList<Codec> clearTextCodecs(const QList<Codec> &options);
// 选的编码格式（0：自动）落到选项上：自动、不在选项里的，两边都用显卡时取第一个（有 HEVC 就是它，没有是 H.264），
// 只能用 CPU 时取 H.264；没有选项时是 0（分享端定）
rcs_codec codecFor(rcs_codec choice, const QList<Codec> &options);

// 按选的填要求里的画面（尺寸、帧率、码率、编码格式、画面格式），别的（指针、声音）不动。codec：codecFor 定好的；
// format：要 4:4:4 时是 RCS_PICTURE_444_8，0 由分享端定（4:2:0）
void apply(uint32_t longSide, uint32_t frameRate, uint32_t bitrateMbps, rcs_codec codec, rcs_picture_format format,
           const Display &display, rcs_view_request_config &request);

} // namespace view_quality
