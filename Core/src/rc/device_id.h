#pragma once

#include <QString>
#include <QStringView>

namespace rc {

// 设备 ID：9 位数字，首位不是 0，在本机随机生成；显示时三位一组（482 915 307）
class DeviceId
{
public:
    DeviceId() = default;

    static DeviceId generate();
    // 用户输入的、存下来的都行：空格和"-"不算；不是 9 位数字时返回无效的 ID
    static DeviceId parse(QStringView text);

    bool isValid() const { return !digits_.isEmpty(); }
    QString digits() const { return digits_; }
    QString display() const;

    friend bool operator==(const DeviceId &, const DeviceId &) = default;

private:
    explicit DeviceId(QString digits);

    QString digits_;
};

} // namespace rc
