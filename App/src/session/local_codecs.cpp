#include "local_codecs.h"

#include <rcsdk/rcsdk.hpp>

#include <QThread>

namespace {

LocalCodecs::Decoders probe()
{
    LocalCodecs::Decoders decoders;
    for (const rcs_codec_implementation &implementation : rcsdk::codecImplementations()) {
        if (implementation.direction == RCS_CODEC_DECODER && implementation.acceleration == RCS_ACCELERATION_HARDWARE
            && (implementation.picture_formats & (1u << RCS_PICTURE_444_8)) != 0)
            decoders.clearText |= 1u << implementation.codec;
    }
    rcs_capabilities capabilities = rcs_capabilities_init();
    if (rcs_get_capabilities(&capabilities) == RCS_OK) {
        decoders.hardware = capabilities.hardware_decoders;
        decoders.software = capabilities.software_decoders;
    }
    return decoders;
}

} // namespace

LocalCodecs::LocalCodecs(QObject *parent)
    : QObject(parent)
    , thread_(QThread::create([this] { probed_ = probe(); }))
{
    connect(thread_.get(), &QThread::finished, this, [this] {
        decoders_ = probed_;
        ready_ = true;
        emit readyChanged();
    });
    thread_->start();
}

// 程序刚打开就退出：等它列完（SDK 打开着的编解码器要关好）
LocalCodecs::~LocalCodecs()
{
    thread_->wait();
}
