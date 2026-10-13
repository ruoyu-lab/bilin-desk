import QtQuick

// 等宽自适应网格：列数随宽度变化，单元宽度取整，避免小数宽度导致换行抖动
Item {
    id: root

    property int count: 0
    property int minCellWidth: 180
    property int cellHeight: 60
    property int spacing: 12

    readonly property int columns: Math.max(1, Math.floor((width + spacing) / (minCellWidth + spacing)))
    readonly property int cellWidth: Math.floor((width - (columns - 1) * spacing) / columns)
    readonly property int rows: Math.ceil(count / columns)

    function cellX(index) { return (index % columns) * (cellWidth + spacing) }
    function cellY(index) { return Math.floor(index / columns) * (cellHeight + spacing) }

    implicitHeight: rows > 0 ? rows * cellHeight + (rows - 1) * spacing : 0
}
