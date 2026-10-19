# 后台服务 BilinService：持有本机身份和被控那一面的设置，开着本机连接等界面来连。没有界面
find_package(Qt6 REQUIRED COMPONENTS Core Network WebSockets)

# 程序的逻辑放在库里，测试也用它
qt_add_library(rcservicecore STATIC
    src/client_rights.h
    src/connection_history.h
    src/connection_history.cpp
    src/control_server.h
    src/control_server.cpp
    src/device_store.h
    src/device_store.cpp
    src/feedback_sender.h
    src/feedback_sender.cpp
    src/lan_discovery.h
    src/lan_discovery.cpp
    src/lan_signaling.h
    src/lan_signaling.cpp
    src/server_link.h
    src/server_link.cpp
    src/service_wiring.h
    src/service_wiring.cpp
    src/share_chat.h
    src/share_chat.cpp
    src/share_displays.h
    src/share_displays.cpp
    src/share_ids.h
    src/share_transport.h
    src/share_transport.cpp
    src/signaling.h
    src/signaling.cpp
    src/updater.h
    src/updater.cpp
)
target_include_directories(rcservicecore PUBLIC src)
target_link_libraries(rcservicecore PUBLIC rccore Qt6::WebSockets)
# 本机连接上来的是谁（client_rights.h）
if(IOS)
    target_sources(rcservicecore PRIVATE src/client_rights_ios.cpp)
elseif(APPLE)
    target_sources(rcservicecore PRIVATE src/client_rights_mac.cpp)
    target_link_libraries(rcservicecore PRIVATE "-framework SystemConfiguration" "-framework CoreFoundation")
elseif(WIN32)
    target_sources(rcservicecore PRIVATE src/client_rights_win.cpp)
    target_link_libraries(rcservicecore PRIVATE advapi32 wtsapi32)
elseif(ANDROID)
    target_sources(rcservicecore PRIVATE src/client_rights_android.cpp)
endif()

# 手机上后台服务在程序自己的进程里（App/src/mobile/local_service.h），只要上面的库；被别人连、装成系统服务那些没有
if(BILIN_MOBILE)
    return()
endif()

# 有人连着时把界面程序拉起来（电脑上界面程序和后台服务是两个程序）
target_sources(rcservicecore PRIVATE src/app_keeper.h src/app_keeper.cpp)

# 用到 SDK 的部分只在程序里（测试不链接 SDK）
qt_add_executable(rcservice
    main.cpp
    src/local_displays.h
    src/local_displays.cpp
    src/share_clipboard.h
    src/share_clipboard.cpp
    src/share_host.h
    src/share_host.cpp
    src/share_thumbnails.h
    src/share_thumbnails.cpp
    src/sharing.h
    src/sharing.cpp
    src/system_mode.h
)
target_link_libraries(rcservice PRIVATE rcservicecore RemoteControl::Core RemoteControl::Clipboard RemoteControl::Power
                      RemoteControl::Thumbnail rcfiles)
# 显示器的名字（local_displays.h）、装好时怎么运行（system_mode.h、agent_gate.h）
if(APPLE)
    target_sources(rcservice PRIVATE src/local_displays_mac.mm src/system_mode_mac.cpp src/agent_gate.h
                   src/agent_gate_mac.cpp)
    target_link_libraries(rcservice PRIVATE "-framework AppKit" "-framework SystemConfiguration")
else()
    # 程序图标：任务管理器、服务列表里看得出是它
    target_sources(rcservice PRIVATE src/local_displays_win.cpp src/system_mode_win.cpp
                   ${CMAKE_SOURCE_DIR}/App/platform/windows/icon.rc)
    target_link_libraries(rcservice PRIVATE RemoteControl::Supervisor wtsapi32 userenv advapi32)
endif()
target_compile_definitions(rcservice PRIVATE APP_VERSION="${BILIN_VERSION}" RC_SERVER_URL="${RC_SERVER_URL}")
# 没有控制台窗口（Windows）；macOS 上放进 .app 里（见 App/CMakeLists.txt）
set_target_properties(rcservice PROPERTIES
    OUTPUT_NAME BilinService
    WIN32_EXECUTABLE TRUE
)

# macOS：程序里嵌一份 Info.plist（platform/macos/Info.plist.in）：系统按它认后台服务、助手，用到界面框架时也不在 Dock 里出现
function(rc_embed_info_plist target identifier)
    set(HELPER_IDENTIFIER ${identifier})
    get_target_property(HELPER_NAME ${target} OUTPUT_NAME)
    set(plist "${CMAKE_CURRENT_BINARY_DIR}/${target}-Info.plist")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/platform/macos/Info.plist.in" "${plist}" @ONLY)
    target_link_options(${target} PRIVATE "LINKER:-sectcreate,__TEXT,__info_plist,${plist}")
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${plist}")
endfunction()
if(APPLE)
    rc_embed_info_plist(rcservice ${service_identifier})
endif()

# macOS 装好时各个会话里的助手（agent/main.cpp）：放进 .app 的 Contents/Helpers（见 App/CMakeLists.txt）
if(APPLE)
    qt_add_executable(rcagent agent/main.cpp)
    target_link_libraries(rcagent PRIVATE rccore RemoteControl::Core RemoteControl::Clipboard RemoteControl::FileTransfer
                          RemoteControl::Power RemoteControl::Thumbnail)
    set_target_properties(rcagent PROPERTIES OUTPUT_NAME BilinAgent)
    rc_embed_info_plist(rcagent ${agent_identifier})
endif()

# Windows：和界面程序装在同一个目录
if(WIN32)
    install(TARGETS rcservice COMPONENT app RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
