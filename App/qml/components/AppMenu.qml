import QtQuick
import QtQuick.Controls.Basic

// 菜单：按最宽的一项定宽，右边带快捷键这样的说明时不挤在一起；至少 160。
// 只量菜单项：小标题、说明（AppMenuLabel）会折行，不撑宽
Menu {
    id: root

    topPadding: 5
    bottomPadding: 5
    width: {
        let widest = 0
        for (let i = 0; i < count; ++i) {
            const item = itemAt(i)
            if (item instanceof MenuItem)
                widest = Math.max(widest, item.implicitWidth)
        }
        return Math.max(160, widest + leftPadding + rightPadding)
    }

    background: PopupSurface {
        radius: 10
    }
}
