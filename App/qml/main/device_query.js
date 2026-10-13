.pragma library

// 设备列表（Devices.devices）的筛选和排序，概览页、设备页用。
// scope：mine（我的设备）| saved（保存了的，不算我的设备）| discovered（附近、没保存、不是我的设备）
//        | nearby（附近、不是我的设备）| favorites | recent
// 我的设备优先：一台设备是我的设备，就只在"我的设备"里，不再出现在附近、已保存里
// filter：{ search, onlyOnline, os, sort: "name" | "online" | "recent" }

function inScope(d, scope) {
    switch (scope) {
    case "mine": return d.mine
    case "saved": return d.saved && !d.mine
    case "discovered": return d.discovered && !d.saved && !d.mine
    case "nearby": return d.discovered && !d.mine
    case "favorites": return d.favorite
    case "recent": return d.lastSeen > 0
    default: return false
    }
}

function query(devices, scope, filter) {
    const search = (filter.search || "").trim().toLowerCase()
    const sort = scope === "recent" ? "recent" : filter.sort
    return devices.filter(d => inScope(d, scope)
                               && (!filter.onlyOnline || d.online)
                               && (!filter.os || d.os === filter.os)
                               && (search.length === 0 || d.name.toLowerCase().includes(search)
                                   || d.address.toLowerCase().includes(search)
                                   || d.deviceId.replace(/\s+/g, "").includes(search.replace(/\s+/g, ""))))
                  .sort((a, b) => {
                      if (sort === "recent")
                          return b.lastSeen - a.lastSeen
                      if (sort === "online" && a.online !== b.online)
                          return a.online ? -1 : 1
                      return a.name.localeCompare(b.name)
                  })
}
