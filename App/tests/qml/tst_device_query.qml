import QtQuick
import QtTest
import "../../qml/main/device_query.js" as Query

TestCase {
    name: "DeviceQuery"

    function device(fields) {
        return Object.assign({ key: fields.deviceId, deviceId: "", address: "", name: "", os: "", online: false,
                               mine: false, favorite: false, saved: false, discovered: false, lastSeen: 0 }, fields)
    }

    readonly property var devices: [
        // 我的设备，同时也在附近、也保存过
        device({ deviceId: "482 915 307", address: "482 915 307", name: "书房的电脑", os: "Windows", online: true,
                 mine: true, saved: true, discovered: true, lastSeen: 300 }),
        device({ deviceId: "193 847 562", address: "192.168.1.20", name: "客厅的 Mac", os: "macOS", online: true,
                 discovered: true }),
        device({ deviceId: "620 113 498", address: "620 113 498", name: "阿明的电脑", os: "Windows", saved: true,
                 favorite: true, lastSeen: 200 }),
        device({ deviceId: "", address: "10.0.0.8", name: "10.0.0.8", saved: true, discovered: true, lastSeen: 100 })
    ]

    function names(list) {
        return list.map(d => d.name)
    }

    // 我的设备只在"我的设备"里，不再出现在附近、已保存里
    function test_mineComesFirst() {
        compare(names(Query.query(devices, "mine", {})), ["书房的电脑"])
        compare(names(Query.query(devices, "saved", { sort: "name" })).indexOf("书房的电脑"), -1)
        compare(names(Query.query(devices, "nearby", { sort: "name" })).indexOf("书房的电脑"), -1)
        compare(names(Query.query(devices, "discovered", { sort: "name" })).indexOf("书房的电脑"), -1)
    }

    // 附近的里已经保存了的不再算"发现的"
    function test_discoveredIsNotSaved() {
        compare(names(Query.query(devices, "discovered", { sort: "name" })), ["客厅的 Mac"])
        compare(Query.query(devices, "nearby", { sort: "name" }).length, 2)
    }

    // 按名字、地址、设备 ID 找；设备 ID 里的空格不算
    function test_search() {
        compare(names(Query.query(devices, "saved", { search: "阿明", sort: "name" })), ["阿明的电脑"])
        compare(names(Query.query(devices, "nearby", { search: "192.168", sort: "name" })), ["客厅的 Mac"])
        compare(names(Query.query(devices, "mine", { search: "482915", sort: "name" })), ["书房的电脑"])
        compare(names(Query.query(devices, "saved", { search: "620 113", sort: "name" })), ["阿明的电脑"])
        compare(Query.query(devices, "saved", { search: "  ", sort: "name" }).length, 2)
    }

    function test_onlineAndSystem() {
        compare(Query.query(devices, "nearby", { onlyOnline: true, sort: "name" }).length, 1)
        compare(names(Query.query(devices, "saved", { os: "Windows", sort: "name" })), ["阿明的电脑"])
    }

    // 在线的在前；最近连过的按时间，新的在前
    function test_sorting() {
        const both = [device({ name: "乙", online: false }), device({ name: "甲", online: true })].map(d => {
            d.saved = true
            return d
        })
        compare(names(Query.query(both, "saved", { sort: "online" })), ["甲", "乙"])
        compare(names(Query.query(devices, "recent", { sort: "name" })), ["书房的电脑", "阿明的电脑", "10.0.0.8"])
        compare(names(Query.query(devices, "favorites", { sort: "name" })), ["阿明的电脑"])
    }
}
