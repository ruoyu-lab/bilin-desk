.pragma library

// 手机上列表里的时间：今天、昨天写几点，今年写几月几日，再早写哪一年
function lastSeen(ms) {
    if (!ms)
        return ""
    const when = new Date(ms)
    const now = new Date()
    const startOfToday = new Date(now.getFullYear(), now.getMonth(), now.getDate()).getTime()
    const time = Qt.formatTime(when, "HH:mm")
    if (ms >= startOfToday)
        return qsTr("今天 %1").arg(time)
    if (ms >= startOfToday - 24 * 3600 * 1000)
        return qsTr("昨天 %1").arg(time)
    if (when.getFullYear() === now.getFullYear())
        return qsTr("%1月%2日").arg(when.getMonth() + 1).arg(when.getDate())
    return qsTr("%1年%2月%3日").arg(when.getFullYear()).arg(when.getMonth() + 1).arg(when.getDate())
}
