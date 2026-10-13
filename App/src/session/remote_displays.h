#pragma once

#include <rc/control_protocol.h>

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <functional>

// 对方的显示器（观看端）：一次看一块。SDK 只在分享端列得出显示器，分享端经数据通道告诉这里有哪几块、
// 正在分享哪一块（rc/control_protocol.h）；选一块就请对方换过去。对方的画面是连着它的所有人共用的：
// 别人换了，这里也跟着变
class RemoteDisplays : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("属于 ViewerSession")

    // [{name, width, height, main, thumbnail}]，按对方桌面上的位置从左到右：name 是对方系统给的名字，系统不说时为空；
    // 尺寸是像素；thumbnail 是要过的缩略图（PNG 的 data: 地址），还没有时为空。没连上时是空的
    Q_PROPERTY(QVariantList items READ items NOTIFY changed)
    // 正在看的是 items 里的第几块
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY changed)

public:
    // send：经数据通道发给对方（rc/peer_protocol.h 的 ControlChannel）
    explicit RemoteDisplays(std::function<void(const QByteArray &message)> send, QObject *parent = nullptr);

    QVariantList items() const;
    int currentIndex() const;

    // 对方经控制通道发来的消息
    void received(const QByteArray &message);
    // 连接结束了
    void clear();

    // 请对方换成分享 items 里的第 index 块
    Q_INVOKABLE void show(int index);
    // 请对方截每一块的缩略图（换显示器之前看一眼），截好一块来一块
    Q_INVOKABLE void requestThumbnails();

signals:
    void changed();

private:
    std::function<void(const QByteArray &)> send_;
    rc::control::DisplayList list_;
    QHash<quint32, QString> thumbnails_; // 按 display_id
};
