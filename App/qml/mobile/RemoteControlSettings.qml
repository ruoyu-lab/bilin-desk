import QtQuick

// 手机、平板设置里的"远程操作"一组：手指怎么操作电脑、连着时屏幕常亮（MobileSettings、TabletMain）
SettingsSection {
    id: root

    // 手指怎么操作电脑：touchpad | direct（没选过时手机是触摸板、平板直接点）
    property string fingerMode: "touchpad"
    // 行前面带图标（平板上是电脑版的设置页，每行都有）
    property bool icons: false

    title: qsTr("远程操作")
    footer: root.fingerMode === "touchpad" ? qsTr("触摸板：手指在屏幕上滑动光标，点一下是点击")
                                           : qsTr("直接点：手指点在哪，就点在电脑屏幕的哪里")
    SettingsRow {
        iconName: root.icons ? "pointer" : ""
        label: qsTr("手指操作方式")
        SegmentedControl {
            width: 170
            model: [qsTr("触摸板"), qsTr("直接点")]
            currentIndex: root.fingerMode === "touchpad" ? 0 : 1
            onCurrentIndexChanged: {
                const mode = currentIndex === 0 ? "touchpad" : "direct"
                if (mode !== root.fingerMode)
                    AppSettings.fingerMode = mode
            }
        }
    }
    SettingsRow {
        iconName: root.icons ? "eye" : ""
        label: qsTr("连接时屏幕常亮")
        AppSwitch {
            checked: AppSettings.keepScreenOn
            onToggled: AppSettings.keepScreenOn = checked
        }
    }
}
