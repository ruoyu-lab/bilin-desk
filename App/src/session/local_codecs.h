#pragma once

#include <QObject>

#include <cstdint>
#include <memory>

class QThread;

// 本机的解码器：哪些编码格式显卡解得了、CPU 解得了，显卡解得了 4:4:4 的（文字更清楚只用它们，CPU 上解太慢）。
// SDK 第一次列的时候要把编解码器挨个打开试一遍（Android 上要好几秒，之后它记着），所以程序一启动就在
// 后台线程里列；列好以前去连的会话先等着（ViewerSession::open），界面不卡
class LocalCodecs : public QObject
{
    Q_OBJECT

public:
    // 都是 (1u << rcs_codec) 的位
    struct Decoders
    {
        uint32_t hardware = 0;
        uint32_t software = 0;
        uint32_t clearText = 0;
    };

    explicit LocalCodecs(QObject *parent = nullptr);
    ~LocalCodecs() override;

    bool ready() const { return ready_; }
    // ready() 以后才有
    Decoders decoders() const { return decoders_; }

signals:
    void readyChanged();

private:
    std::unique_ptr<QThread> thread_;
    Decoders probed_;   // 后台线程写，它结束以后界面线程读
    Decoders decoders_;
    bool ready_ = false;
};
