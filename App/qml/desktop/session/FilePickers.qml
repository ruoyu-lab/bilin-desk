import QtQuick
import QtQuick.Dialogs

// 文件页"发送文件""发送文件夹"选文件（FilesTab）：系统的文件、文件夹对话框。Android 上是 qml/android/FilePickers.qml
Item {
    id: root

    signal chosen(var urls)

    function chooseFiles() {
        filePicker.open()
    }
    function chooseFolder() {
        folderPicker.open()
    }

    FileDialog {
        id: filePicker
        title: qsTr("选择要发送的文件")
        fileMode: FileDialog.OpenFiles
        onAccepted: root.chosen(selectedFiles)
    }

    FolderDialog {
        id: folderPicker
        title: qsTr("选择要发送的文件夹")
        onAccepted: root.chosen([selectedFolder])
    }
}
