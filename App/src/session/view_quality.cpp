#include "view_quality.h"

#include <algorithm>
#include <cmath>

namespace view_quality {

namespace {

// 比原始小的几档长边、比刷新率低的几档帧率
constexpr uint32_t LongSides[] = {3840, 2560, 1920, 1600, 1280, 960};
constexpr uint32_t FrameRates[] = {120, 60, 30, 15};
// 码率：每个像素每帧 0.1 bit，最少 4 Mbit/s、最多 150 Mbit/s（分享端默认最多照办 200 Mbit/s）
constexpr double BitsPerPixel = 0.1;
constexpr uint32_t MinBitrate = 4'000'000;
constexpr uint32_t MaxBitrate = 150'000'000;
// 码率的几档（Mbit/s）
constexpr uint32_t Bitrates[] = {2, 5, 10, 20, 40, 80, 100, 150};
// 编码格式按这个顺序列：HEVC 首选（同样的码率画面比 H.264 好，显卡普遍编解得了）；H.264 哪里都有；再是 AV1、VP9
constexpr rcs_codec Codecs[] = {RCS_CODEC_HEVC, RCS_CODEC_H264, RCS_CODEC_AV1, RCS_CODEC_VP9};

uint32_t longSideOf(const Display &display)
{
    return std::max(display.width, display.height);
}

uint32_t refreshOf(const Display &display)
{
    return display.refreshHz > 0 ? display.refreshHz : 60;
}

// SDK 要的画面尺寸是偶数
uint32_t even(double pixels)
{
    return uint32_t(std::lround(pixels)) & ~1u;
}

} // namespace

QList<uint32_t> longSides(const Display &display)
{
    if (display.width == 0)
        return {};
    QList<uint32_t> sides{0};
    for (const uint32_t side : LongSides) {
        if (side < longSideOf(display))
            sides.append(side);
    }
    return sides;
}

QList<uint32_t> frameRates(const Display &display)
{
    QList<uint32_t> rates{refreshOf(display)};
    for (const uint32_t rate : FrameRates) {
        if (rate < refreshOf(display))
            rates.append(rate);
    }
    return rates;
}

int longSideIndex(uint32_t longSide, const Display &display)
{
    const QList<uint32_t> sides = longSides(display);
    if (longSide == 0 || longSide >= longSideOf(display))
        return sides.isEmpty() ? -1 : 0;
    for (int i = 1; i < sides.size(); ++i) {
        if (sides.at(i) <= longSide)
            return i;
    }
    return int(sides.size()) - 1;
}

int frameRateIndex(uint32_t frameRate, const Display &display)
{
    const QList<uint32_t> rates = frameRates(display);
    if (frameRate == 0 || frameRate >= rates.first())
        return 0;
    for (int i = 1; i < rates.size(); ++i) {
        if (rates.at(i) <= frameRate)
            return i;
    }
    return int(rates.size()) - 1;
}

QList<uint32_t> bitrates()
{
    QList<uint32_t> list{0};
    for (const uint32_t mbps : Bitrates)
        list.append(mbps);
    return list;
}

int bitrateIndex(uint32_t mbps)
{
    const QList<uint32_t> list = bitrates();
    if (mbps == 0)
        return 0;
    for (int i = int(list.size()) - 1; i > 0; --i) {
        if (list.at(i) <= mbps)
            return i;
    }
    return 1;
}

QList<Codec> codecs(uint32_t peerOffer, uint32_t peerHardware, uint32_t localHardware, uint32_t localSoftware,
                    uint32_t clearText)
{
    const auto has = [](uint32_t bits, rcs_codec codec) { return (bits & (1u << codec)) != 0; };
    QList<Codec> hardware;
    QList<Codec> any;
    for (const rcs_codec codec : Codecs) {
        if (!has(peerOffer, codec) || !(has(localHardware, codec) || has(localSoftware, codec)))
            continue;
        const bool both = has(peerHardware, codec) && has(localHardware, codec);
        any.append({codec, both, false});
        if (both)
            hardware.append({codec, true, has(clearText, codec)});
    }
    return hardware.isEmpty() ? any : hardware;
}

QList<Codec> clearTextCodecs(const QList<Codec> &options)
{
    QList<Codec> list;
    for (const Codec &option : options) {
        if (option.clearText)
            list.append(option);
    }
    return list;
}

rcs_codec codecFor(rcs_codec choice, const QList<Codec> &options)
{
    if (options.isEmpty())
        return 0;
    for (const Codec &option : options) {
        if (option.codec == choice)
            return choice;
    }
    // 自动：两边都用显卡的按选项的顺序（HEVC、H.264……）；只能用 CPU 时挑 H.264（CPU 上编解它最省）
    if (options.first().hardware)
        return options.first().codec;
    for (const Codec &option : options) {
        if (option.codec == RCS_CODEC_H264)
            return RCS_CODEC_H264;
    }
    return options.first().codec;
}

void apply(uint32_t longSide, uint32_t frameRate, uint32_t bitrateMbps, rcs_codec codec, rcs_picture_format format,
           const Display &display, rcs_view_request_config &request)
{
    request.width = 0;
    request.height = 0;
    request.frames_per_second = 0;
    request.bitrate_bps = 0;
    request.codec = codec;
    request.format = format;
    // 还不知道对方的显示器：尺寸、帧率由分享端定（1920×1080、60）
    if (display.width == 0)
        return;
    const uint32_t side = longSides(display).at(longSideIndex(longSide, display));
    const double scale = side == 0 ? 1.0 : double(side) / longSideOf(display);
    // 编码格式放不下这么大时分享端按比例缩小（H.264 最大 4096×2304）
    request.width = even(display.width * scale);
    request.height = even(display.height * scale);
    request.frames_per_second = frameRates(display).at(frameRateIndex(frameRate, display));
    if (bitrateMbps > 0) {
        request.bitrate_bps = bitrates().at(bitrateIndex(bitrateMbps)) * 1'000'000;
        return;
    }
    const double bits = double(request.width) * request.height * request.frames_per_second * BitsPerPixel;
    request.bitrate_bps = uint32_t(std::clamp(bits, double(MinBitrate), double(MaxBitrate)));
}

} // namespace view_quality
