import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Popup {
    id: root

    signal connectRequested(var device)

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: 380
    padding: 22
    modal: true
    focus: true

    Overlay.modal: Rectangle { color: Theme.scrim }
    background: PopupSurface { radius: 14 }

    onOpened: {
        address.clear()
        label.clear()
        error.text = ""
        address.forceActiveFocus()
    }

    function submit(connect) {
        const value = address.text.trim()
        if (value.length === 0) {
            error.text = qsTr("请输入设备 ID 或 IP 地址")
            address.forceActiveFocus()
            return
        }
        const device = Devices.ensure(value, label.text.trim())
        if (!device.key) {
            error.text = qsTr("要填 9 位设备 ID，或者同一网络里的 IP 地址")
            address.forceActiveFocus()
            return
        }
        if (connect)
            root.connectRequested(device)
        else
            Devices.save(device.key)
        root.close()
    }

    contentItem: ColumnLayout {
        spacing: 8

        Text {
            Layout.bottomMargin: 8
            text: qsTr("添加设备")
            font.pixelSize: 16
            font.weight: Font.DemiBold
            color: Theme.text
        }

        Text {
            text: qsTr("设备 ID 或地址")
            font.pixelSize: 12
            color: Theme.textSecondary
        }
        AppTextField {
            id: address
            Layout.fillWidth: true
            placeholderText: "482 915 307 / 192.168.1.8"
            onTextChanged: error.text = ""
            onAccepted: root.submit(true)
        }

        Text {
            Layout.topMargin: 6
            text: qsTr("名称（可选）")
            font.pixelSize: 12
            color: Theme.textSecondary
        }
        AppTextField {
            id: label
            Layout.fillWidth: true
            placeholderText: qsTr("书房电脑")
            onAccepted: root.submit(true)
        }

        Text {
            id: error
            visible: text.length > 0
            font.pixelSize: 12
            color: Theme.danger
        }

        RowLayout {
            Layout.topMargin: 12
            spacing: 8
            Item { Layout.fillWidth: true }
            AppButton { text: qsTr("取消"); onClicked: root.close() }
            AppButton { text: qsTr("保存"); onClicked: root.submit(false) }
            AppButton { primary: true; text: qsTr("连接"); onClicked: root.submit(true) }
        }
    }
}
