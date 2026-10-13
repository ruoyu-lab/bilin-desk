#include "session/view_quality.h"

#include <QTest>

using view_quality::Display;

class TestViewQuality : public QObject
{
    Q_OBJECT

private slots:
    // 选项不超过对方显示器：原始分辨率在最前，后面只有比它小的；帧率从刷新率往下
    void optionsStayWithinTheDisplay()
    {
        const Display qhd{2560, 1440, 144};
        QCOMPARE(view_quality::longSides(qhd), (QList<uint32_t>{0, 1920, 1600, 1280, 960}));
        QCOMPARE(view_quality::frameRates(qhd), (QList<uint32_t>{144, 120, 60, 30, 15}));
        const Display laptop{1280, 800, 0};
        QCOMPARE(view_quality::longSides(laptop), (QList<uint32_t>{0, 960}));
        QCOMPARE(view_quality::frameRates(laptop), (QList<uint32_t>{60, 30, 15}));
        QVERIFY(view_quality::longSides({}).isEmpty());
    }

    // 记着的选择落到这一台上：大了取原始，不在选项里的取不超过它的
    void choicesFallToTheNearestOption()
    {
        const Display laptop{1280, 800, 60};
        QCOMPARE(view_quality::longSideIndex(1920, laptop), 0);
        QCOMPARE(view_quality::longSideIndex(0, laptop), 0);
        QCOMPARE(view_quality::longSideIndex(1000, laptop), 1);
        QCOMPARE(view_quality::longSideIndex(500, laptop), 1);
        QCOMPARE(view_quality::frameRateIndex(120, laptop), 0);
        QCOMPARE(view_quality::frameRateIndex(30, laptop), 1);
        QCOMPARE(view_quality::frameRateIndex(0, laptop), 0);
    }

    // 按对方显示器的比例缩小（偶数），帧率不超过刷新率，码率跟着定
    void requestsWhatWasChosen()
    {
        rcs_view_request_config request = rcs_view_request_config_init();
        const Display portrait{1440, 2560, 60};
        view_quality::apply(1920, 120, 0, 0, 0, portrait, request);
        QCOMPARE(request.width, 1080u);
        QCOMPARE(request.height, 1920u);
        QCOMPARE(request.frames_per_second, 60u);
        QVERIFY(request.bitrate_bps >= 4'000'000u && request.bitrate_bps <= 150'000'000u);

        view_quality::apply(0, 0, 0, 0, 0, {3840, 2160, 60}, request);
        QCOMPARE(request.width, 3840u);
        QCOMPARE(request.frames_per_second, 60u);
        QCOMPARE(request.bitrate_bps, 49'766'400u);

        // 还不知道显示器：由分享端定
        view_quality::apply(1920, 60, 0, 0, 0, {}, request);
        QCOMPARE(request.width, 0u);
        QCOMPARE(request.frames_per_second, 0u);
    }

    // 选了码率：就是它（不在选项里的取不超过它的）；编码格式原样放进要求里
    void requestsTheChosenBitrateAndCodec()
    {
        rcs_view_request_config request = rcs_view_request_config_init();
        view_quality::apply(0, 0, 20, RCS_CODEC_HEVC, 0, {2560, 1440, 60}, request);
        QCOMPARE(request.bitrate_bps, 20'000'000u);
        QCOMPARE(request.codec, rcs_codec(RCS_CODEC_HEVC));
        view_quality::apply(0, 0, 30, 0, 0, {2560, 1440, 60}, request);
        QCOMPARE(request.bitrate_bps, 20'000'000u);
        QCOMPARE(request.codec, rcs_codec(0));
        QCOMPARE(request.format, rcs_picture_format(0));
        view_quality::apply(0, 0, 0, RCS_CODEC_HEVC, RCS_PICTURE_444_8, {2560, 1440, 60}, request);
        QCOMPARE(request.format, rcs_picture_format(RCS_PICTURE_444_8));
        QCOMPARE(view_quality::bitrates().first(), 0u);
        QCOMPARE(view_quality::bitrateIndex(0), 0);
        QCOMPARE(view_quality::bitrateIndex(1), 1);
        QCOMPARE(view_quality::bitrateIndex(500), int(view_quality::bitrates().size()) - 1);
    }

    // 两边都用显卡的有就只列它们；一个都没有时才列要用 CPU 的
    void codecsPreferHardwareOnBothEnds()
    {
        using view_quality::Codec;
        const uint32_t h264 = 1u << RCS_CODEC_H264, hevc = 1u << RCS_CODEC_HEVC, av1 = 1u << RCS_CODEC_AV1;
        // 对方三种都提供，H.264、HEVC 用显卡编；本机三种都能用显卡解：HEVC 排第一
        QCOMPARE(view_quality::codecs(h264 | hevc | av1, h264 | hevc, h264 | hevc | av1, 0, 0),
                 (QList<Codec>{{RCS_CODEC_HEVC, true}, {RCS_CODEC_H264, true}}));
        // 对方没有显卡编码器（虚拟机）：只能 CPU
        QCOMPARE(view_quality::codecs(h264, 0, h264 | hevc, 0, 0), (QList<Codec>{{RCS_CODEC_H264, false}}));
        // 本机解不了的不列
        QCOMPARE(view_quality::codecs(h264 | av1, h264 | av1, h264, 0, 0), (QList<Codec>{{RCS_CODEC_H264, true}}));
        QVERIFY(view_quality::codecs(av1, av1, h264, 0, 0).isEmpty());
        // VP9 排在最后；对方用显卡编不了它（Mac、NVIDIA）就不列
        const uint32_t vp9 = 1u << RCS_CODEC_VP9;
        QCOMPARE(view_quality::codecs(h264 | vp9, h264 | vp9, h264 | vp9, 0, 0),
                 (QList<Codec>{{RCS_CODEC_H264, true}, {RCS_CODEC_VP9, true}}));
        QCOMPARE(view_quality::codecs(h264 | vp9, h264, h264 | vp9, 0, 0), (QList<Codec>{{RCS_CODEC_H264, true}}));

        // 4:4:4 只算两边都用显卡的
        QCOMPARE(view_quality::codecs(h264 | hevc, h264 | hevc, h264 | hevc, 0, hevc),
                 (QList<Codec>{{RCS_CODEC_HEVC, true, true}, {RCS_CODEC_H264, true, false}}));
        QCOMPARE(view_quality::codecs(hevc, 0, hevc, 0, hevc), (QList<Codec>{{RCS_CODEC_HEVC, false, false}}));
        QCOMPARE(view_quality::clearTextCodecs({{RCS_CODEC_H264, true, false}, {RCS_CODEC_HEVC, true, true}}),
                 (QList<Codec>{{RCS_CODEC_HEVC, true, true}}));

        // 自动：两边都用显卡的 HEVC；没有 HEVC 时 H.264；只能用 CPU 时 H.264
        const QList<Codec> options{{RCS_CODEC_HEVC, true}, {RCS_CODEC_H264, true}};
        QCOMPARE(view_quality::codecFor(0, options), rcs_codec(RCS_CODEC_HEVC));
        QCOMPARE(view_quality::codecFor(RCS_CODEC_H264, options), rcs_codec(RCS_CODEC_H264));
        QCOMPARE(view_quality::codecFor(RCS_CODEC_AV1, options), rcs_codec(RCS_CODEC_HEVC));
        QCOMPARE(view_quality::codecFor(0, {{RCS_CODEC_H264, true}, {RCS_CODEC_AV1, true}}), rcs_codec(RCS_CODEC_H264));
        QCOMPARE(view_quality::codecFor(0, {{RCS_CODEC_HEVC, false}, {RCS_CODEC_H264, false}}), rcs_codec(RCS_CODEC_H264));
        QCOMPARE(view_quality::codecFor(RCS_CODEC_HEVC, {}), rcs_codec(0));
    }
};

QTEST_GUILESS_MAIN(TestViewQuality)
#include "tst_view_quality.moc"
