import QtQuick

// 自定义标题栏区域：拖动移动窗口，双击缩放（手机、平板上窗口不能拖，不起作用）
Item {
    id: root

    enabled: !Theme.touch

    DragHandler {
        target: null
        onActiveChanged: if (active) root.Window.window.startSystemMove()
    }

    TapHandler {
        onDoubleTapped: {
            const w = root.Window.window
            if (w.visibility === Window.Maximized)
                w.showNormal()
            else
                w.showMaximized()
        }
    }
}
