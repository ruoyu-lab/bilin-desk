#include "local_displays.h"

#include <rcsdk/rcsdk.hpp>

#include <algorithm>

QList<rc::control::Display> localDisplays(const rcsdk::Session &session)
{
    std::vector<rcs_display_info> displays = session.displays();
    std::sort(displays.begin(), displays.end(), [](const rcs_display_info &a, const rcs_display_info &b) {
        return std::pair(a.desktop_x, a.desktop_y) < std::pair(b.desktop_x, b.desktop_y);
    });
    const QHash<quint32, QString> names = displayNames();
    QList<rc::control::Display> list;
    for (const rcs_display_info &display : displays) {
        list.append({display.display_id, names.value(display.display_id), int(display.pixel_width),
                     int(display.pixel_height), display.is_main != 0});
    }
    return list;
}
