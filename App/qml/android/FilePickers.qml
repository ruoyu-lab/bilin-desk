import QtQuick

// 文件页"发送文件"选文件（FilesTab）：Android 上直接调系统的文件选择器（AndroidFilePicker），
// 不用 QtQuick.Dialogs；文件夹选不了（FilesTab 里没有那个按钮）
Item {
    id: root

    signal chosen(var urls)

    function chooseFiles() {
        picker.open()
    }

    AndroidFilePicker {
        id: picker
        onAccepted: urls => root.chosen(urls)
    }
}
