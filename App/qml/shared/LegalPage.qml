import QtQuick
import QtQuick.Layouts

// 设置 › 关于比邻 里的服务协议、隐私政策：全文，程序里带着（App/legal，Documents 读）
SettingsScroll {
    id: root

    // terms | privacy
    property string kind

    title: kind === "privacy" ? qsTr("隐私政策") : qsTr("服务协议")
    backTo: qsTr("关于比邻")

    onKindChanged: scrollToTop()

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: document.implicitHeight + 36
        radius: Theme.cardRadius
        color: Theme.card

        LegalText {
            id: document
            x: 18
            y: 18
            width: parent.width - 36
            kind: root.kind
        }
    }
}
