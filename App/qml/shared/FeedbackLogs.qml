pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

// 设置 › 反馈问题 › 日志：提交反馈时带上的那几份（Feedback.logs），点一份看全文（FeedbackLog）
SettingsScroll {
    id: root

    title: qsTr("日志")
    backTo: qsTr("反馈问题")

    signal logRequested(int index)

    Text {
        Layout.fillWidth: true
        text: qsTr("提交时带上的就是这些：比邻这次和上次运行时记下的连接、出错的情况，每份最多最后 512 KB。")
        wrapMode: Text.Wrap
        font.pixelSize: 13
        color: Theme.textSecondary
    }
    SettingsSection {
        Repeater {
            model: Feedback.logs
            delegate: SettingsRow {
                required property var modelData
                required property int index
                label: modelData.name
                value: modelData.size
                navigates: true
                onClicked: root.logRequested(index)
            }
        }
    }
}
