import QtQuick

QtObject {
    property string view: "home"
    property string selectedId
    property bool detailOpen: true
    property string search
    property string sort: "name"
    property bool onlyOnline: false
    property string os

    readonly property bool filtering: onlyOnline || os.length > 0

    function showDevice(listView, key) {
        view = listView
        selectedId = key
    }
}
