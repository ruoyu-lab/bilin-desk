import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts
import QtCore

// 电脑版设置页"通用"里的一行（SettingsPage 的 generalRows）：收到的文件存到哪个文件夹
ColumnLayout {
    id: root

    readonly property url downloads: StandardPaths.writableLocation(StandardPaths.DownloadLocation)
    // 收到的文件存到的文件夹，显示它的名字
    readonly property string folderName: {
        const url = AppSettings.downloadFolder.toString()
        return url.length > 0 ? decodeURIComponent(url.replace(/\/$/, "").split("/").pop()) : qsTr("下载")
    }

    spacing: 0

    SettingsRow {
        iconName: "folder"
        label: qsTr("收到的文件存到")
        value: root.folderName
        navigates: true
        onClicked: folderPicker.open()
    }

    // 选了系统的"下载"就记成默认，不记具体的路径
    FolderDialog {
        id: folderPicker
        title: qsTr("收到的文件存到")
        currentFolder: AppSettings.downloadFolder.toString().length > 0 ? AppSettings.downloadFolder : root.downloads
        onAccepted: AppSettings.downloadFolder = selectedFolder.toString() === root.downloads.toString() ? "" : selectedFolder
    }
}
