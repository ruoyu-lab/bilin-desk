# App 是仓库根目录 CMakeLists.txt 的子目录（那里定义项目），不能单独配置
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    message(FATAL_ERROR "请打开仓库根目录的 CMakeLists.txt，而不是 App/CMakeLists.txt")
endif()

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 Network ShaderTools GuiPrivate Svg)

# 电脑版、手机版共用的
set(app_sources
    src/account/account.h
    src/account/account.cpp
    src/appearance.h
    src/appearance.cpp
    src/chat/conversation.h
    src/chat/conversation.cpp
    src/chat/conversations.h
    src/chat/conversations.cpp
    src/clipboard.h
    src/clipboard.cpp
    src/devices/device_directory.h
    src/devices/device_directory.cpp
    src/devices/device_wake.h
    src/devices/device_wake.cpp
    src/documents.h
    src/documents.cpp
    src/files/file_transfer_list.h
    src/files/file_transfer_list.cpp
    src/remote_view/remote_input.h
    src/remote_view/remote_input.cpp
    src/remote_view/remote_view.h
    src/remote_view/remote_view.cpp
    src/remote_view/view_graphics.h
    src/remote_view/view_graphics.cpp
    src/service_client.h
    src/service_client.cpp
    src/session/frame_mailbox.h
    src/session/frame_mailbox.cpp
    src/session/local_codecs.h
    src/session/local_codecs.cpp
    src/session/remote_displays.h
    src/session/remote_displays.cpp
    src/session/view_quality.h
    src/session/view_quality.cpp
    src/session/viewer_audio.h
    src/session/viewer_audio.cpp
    src/session/viewer_chat.h
    src/session/viewer_chat.cpp
    src/session/viewer_clipboard.h
    src/session/viewer_clipboard.cpp
    src/session/viewer_power.h
    src/session/viewer_power.cpp
    src/session/viewer_files.h
    src/session/viewer_files.cpp
    src/session/viewer_session.h
    src/session/viewer_session.cpp
    src/session/viewer_sessions.h
    src/session/viewer_sessions.cpp
    src/this_device.h
    src/this_device.cpp
)
set(app_qml
    qml/AppSettings.qml
    qml/Theme.qml
    qml/components/AdaptiveGrid.qml
    qml/components/AppButton.qml
    qml/components/AppComboBox.qml
    qml/components/AppMenu.qml
    qml/components/AppMenuItem.qml
    qml/components/AppMenuLabel.qml
    qml/components/AppMenuSeparator.qml
    qml/components/AppSwitch.qml
    qml/components/AppTextField.qml
    qml/components/Avatar.qml
    qml/components/ChatView.qml
    qml/components/DeviceIcon.qml
    qml/components/FileOfferCard.qml
    qml/components/Icon.qml
    qml/components/Logo.qml
    qml/components/IconButton.qml
    qml/components/LegalText.qml
    qml/components/NavItem.qml
    qml/components/PopupSurface.qml
    qml/components/SearchBox.qml
    qml/components/SegmentedControl.qml
    qml/components/SettingsRow.qml
    qml/components/SettingsScroll.qml
    qml/components/SettingsSection.qml
    qml/components/StatusDot.qml
    qml/components/ToolGroup.qml
    qml/components/TransferRow.qml
    qml/components/UnderlineTab.qml
    qml/main/AboutSettings.qml
    qml/main/AccountDialog.qml
    qml/main/AccountSettings.qml
    qml/main/LegalPage.qml
    qml/main/LicensesPage.qml
    qml/session/RemoteCanvas.qml
    qml/session/ChatTab.qml
    qml/session/FilesTab.qml
    qml/session/SettingsTab.qml
    qml/session/SidePanel.qml
    qml/auth/LoginForm.qml
    qml/main/device_query.js
    qml/main/DeviceMenu.qml
)
# 电脑版才有的：安装、托盘、开机启动、单实例、窗口外框、被别人连（share）
set(desktop_sources
    src/app_lifecycle.h
    src/app_lifecycle.cpp
    src/autostart.h
    src/autostart.cpp
    src/cursor_position.h
    src/cursor_position.cpp
    src/dock.h
    src/login_item.h
    src/quit_reason.h
    src/setup/setup.h
    src/setup/setup.cpp
    src/setup/setup_platform.h
    src/setup/setup_steps.h
    src/setup/setup_steps.cpp
    src/share/incoming_chat.h
    src/share/incoming_chat.cpp
    src/share/incoming_connections.h
    src/share/incoming_connections.cpp
    src/share/incoming_files.h
    src/share/incoming_files.cpp
    src/share/incoming_history.h
    src/share/incoming_history.cpp
    src/single_instance.h
    src/single_instance.cpp
    src/windowchrome.h
)
# 解出的 GPU 画面直接画（Mac 的 Metal、Windows 的 D3D11 纹理）；手机上先用 CPU 上的图像（RemoteView）
set(gpu_frame_sources
    src/remote_view/frame_node.h
    src/remote_view/frame_node.cpp
    src/remote_view/frame_textures.h
)
set(desktop_qml
    qml/BrowseState.qml
    qml/Main.qml
    qml/auth/LoginArt.qml
    qml/auth/LoginWindow.qml
    qml/components/WindowButtons.qml
    qml/components/WindowDragArea.qml
    qml/components/WindowMemory.qml
    qml/main/AccountPopup.qml
    qml/main/AddDeviceDialog.qml
    qml/main/DeviceDetail.qml
    qml/main/DeviceRow.qml
    qml/main/DeviceTile.qml
    qml/main/DevicesPage.qml
    qml/main/MainToolbar.qml
    qml/main/MainWindow.qml
    qml/main/OverviewPage.qml
    qml/main/SettingsPage.qml
    qml/main/Sidebar.qml
    qml/setup/PermissionWindow.qml
    qml/setup/SetupWindow.qml
    qml/share/IncomingWindow.qml
    qml/share/ShareChatWindow.qml
    qml/session/CompactChrome.qml
    qml/session/PowerConfirm.qml
    qml/session/PowerMenu.qml
    qml/session/DisplayPicker.qml
    qml/session/DisplayThumbnails.qml
    qml/session/SessionTabs.qml
    qml/session/SessionTools.qml
    qml/session/SessionView.qml
    qml/session/SessionWindow.qml
)
# 手机版（BILIN_MOBILE）：一个窗口、底部标签栏（宽的时候侧栏），后台服务在本进程里
set(mobile_sources
    src/mobile/local_service.h
    src/mobile/local_service.cpp
    src/mobile/screen_awake.h
    src/mobile/screen_awake.cpp
)
set(mobile_qml
    qml/mobile/MobileMain.qml
    qml/mobile/MobilePage.qml
    qml/mobile/MobileTabBar.qml
    qml/mobile/MobileNavRail.qml
    qml/mobile/MobileOverview.qml
    qml/mobile/MobileDevices.qml
    qml/mobile/MobileRecent.qml
    qml/mobile/MobileSettings.qml
    qml/mobile/MobileDeviceRow.qml
    qml/mobile/MobileLogin.qml
    qml/mobile/MobileSession.qml
    qml/mobile/mobile_format.js
)

if(BILIN_MOBILE)
    qt_add_executable(Bilin main_mobile.cpp)
    list(APPEND app_sources ${mobile_sources})
    list(APPEND app_qml ${mobile_qml})
else()
    find_package(Qt6 REQUIRED COMPONENTS LabsPlatform)
    qt_add_executable(Bilin main.cpp)
    list(APPEND app_sources ${desktop_sources} ${gpu_frame_sources})
    list(APPEND app_qml ${desktop_qml})
endif()

set_source_files_properties(qml/Theme.qml qml/AppSettings.qml
                            PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(Bilin
    URI Bilin
    SOURCES ${app_sources}
    QML_FILES ${app_qml}
    RESOURCES
        branding/logo.svg
        qml/icons/apple.svg
        qml/icons/bell.svg
        qml/icons/camera.svg
        qml/icons/check.svg
        qml/icons/chevron-down.svg
        qml/icons/chevron-right.svg
        qml/icons/clock.svg
        qml/icons/copy.svg
        qml/icons/download.svg
        qml/icons/eye.svg
        qml/icons/file.svg
        qml/icons/filter.svg
        qml/icons/folder.svg
        qml/icons/home.svg
        qml/icons/info.svg
        qml/icons/key.svg
        qml/icons/keyboard.svg
        qml/icons/link.svg
        qml/icons/log-out.svg
        qml/icons/mail.svg
        qml/icons/maximize.svg
        qml/icons/message.svg
        qml/icons/minimize.svg
        qml/icons/monitor.svg
        qml/icons/panel-right.svg
        qml/icons/palette.svg
        qml/icons/plus.svg
        qml/icons/pointer.svg
        qml/icons/power.svg
        qml/icons/radar.svg
        qml/icons/refresh.svg
        qml/icons/search.svg
        qml/icons/send.svg
        qml/icons/settings.svg
        qml/icons/sidebar.svg
        qml/icons/sort.svg
        qml/icons/star-filled.svg
        qml/icons/star.svg
        qml/icons/terminal.svg
        qml/icons/tray-mac.png
        qml/icons/tray.png
        qml/icons/trash.svg
        qml/icons/upload.svg
        qml/icons/user.svg
        qml/icons/volume.svg
        qml/icons/window-maximize.svg
        qml/icons/window-minimize.svg
        qml/icons/window-restore.svg
        qml/icons/windows.svg
        qml/icons/x.svg
)

# 开源声明里的许可全文：本程序和用到的第三方代码各自的许可（Documents 读它们）
set_source_files_properties("${PROJECT_SOURCE_DIR}/LICENSE" PROPERTIES QT_RESOURCE_ALIAS AGPL-3.0.txt)
set_source_files_properties("${PROJECT_SOURCE_DIR}/third_party/qwindowkit/LICENSE" PROPERTIES QT_RESOURCE_ALIAS QWindowKit.txt)
set_source_files_properties("${PROJECT_SOURCE_DIR}/third_party/rcsdk/LICENSE" PROPERTIES QT_RESOURCE_ALIAS RemoteControlSDK.txt)
set_source_files_properties("${PROJECT_SOURCE_DIR}/third_party/rcsdk/THIRD_PARTY_NOTICES.md"
                            PROPERTIES QT_RESOURCE_ALIAS RemoteControlSDK-third-party.txt)
qt_add_resources(Bilin "licenses"
    PREFIX "/licenses"
    BASE licenses
    FILES
        "${PROJECT_SOURCE_DIR}/LICENSE"
        "${PROJECT_SOURCE_DIR}/third_party/qwindowkit/LICENSE"
        "${PROJECT_SOURCE_DIR}/third_party/rcsdk/LICENSE"
        "${PROJECT_SOURCE_DIR}/third_party/rcsdk/THIRD_PARTY_NOTICES.md"
        licenses/GPL-3.0.txt
        licenses/LGPL-3.0.txt
        licenses/Lucide.txt
)
qt_add_resources(Bilin "legal"
    PREFIX "/legal"
    BASE legal
    FILES
        legal/terms.md
        legal/privacy.md
)

# 远程画面的着色器：编成各图形接口都能用的 .qsb，放进资源（:/shaders/）
qt_add_shaders(Bilin "shaders"
    PREFIX "/shaders"
    BASE src/remote_view/shaders
    FILES
        src/remote_view/shaders/frame.vert
        src/remote_view/shaders/nv12.frag
        src/remote_view/shaders/bgra.frag
)

# QML 类型注册按文件名包含头文件
target_include_directories(Bilin PRIVATE src src/account src/chat src/devices src/files src/remote_view src/session
                           src/setup src/share src/mobile)
target_compile_definitions(Bilin PRIVATE APP_VERSION="${BILIN_VERSION}")

# Qt6::Svg：界面里的图标都是 .svg，运行时靠 SVG 的图片格式插件（imageformats/qsvg）加载。部署工具只有看到程序链接了
# QtSvg 才会把这个插件带进安装包，不链接的话打成包以后图标全是空的（开发时用的是 Qt 安装目录里的插件，看不出来）
target_link_libraries(Bilin
    PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::Network Qt6::Svg Qt6::GuiPrivate rccore rcfiles RemoteControl::Core RemoteControl::Clipboard RemoteControl::Power
)

# 手机版：后台服务的逻辑编进程序里（LocalService），服务器地址也是。Android 打成 APK（platform/android：清单、名字、
# 图标），版本号 0.1.0 记成 100（每次发布数字变大）
if(BILIN_MOBILE)
    target_link_libraries(Bilin PRIVATE rcservicecore)
    target_compile_definitions(Bilin PRIVATE RC_SERVER_URL="${RC_SERVER_URL}")
    # 苹果系统上是 .app 包（iOS 必须是；Mac 上预览时也是，不然程序和 QML 模块的目录 Bilin/ 同名）
    set_target_properties(Bilin PROPERTIES MACOSX_BUNDLE TRUE)
    if(ANDROID)
        math(EXPR android_version_code "${PROJECT_VERSION_MAJOR} * 10000 + ${PROJECT_VERSION_MINOR} * 100 + ${PROJECT_VERSION_PATCH}")
        set_target_properties(Bilin PROPERTIES
            QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/platform/android"
            QT_ANDROID_VERSION_NAME "${BILIN_VERSION}"
            QT_ANDROID_VERSION_CODE ${android_version_code}
            QT_ANDROID_MIN_SDK_VERSION 29
            QT_ANDROID_TARGET_SDK_VERSION 35
        )
        # Qt 在 Android 上的加密连接（连正式服务器的 wss://）靠 OpenSSL，Qt 不带：用 Android SDK 目录里 KDAB 的
        # android_openssl（Qt 的文档推荐，Qt Creator 装 Android 开发环境时一起下好）打进 APK，Qt 运行时自己加载。
        # 它的许可（Apache-2.0）进开源声明
        set(android_openssl "${ANDROID_SDK_ROOT}/android_openssl")
        set(android_openssl_libs "${android_openssl}/ssl_3/${CMAKE_ANDROID_ARCH_ABI}")
        if(NOT EXISTS "${android_openssl_libs}/libssl_3.so")
            message(FATAL_ERROR "没有 Android 的 OpenSSL（${android_openssl_libs}）：在 Qt Creator 的 Android 设置里装，"
                                "或者把 https://github.com/KDAB/android_openssl 克隆到 Android SDK 目录下")
        endif()
        set_property(TARGET Bilin APPEND PROPERTY QT_ANDROID_EXTRA_LIBS
                     "${android_openssl_libs}/libcrypto_3.so" "${android_openssl_libs}/libssl_3.so")
        set_source_files_properties("${android_openssl}/LICENSE" PROPERTIES QT_RESOURCE_ALIAS OpenSSL.txt)
        qt_add_resources(Bilin "licenses_android"
            PREFIX "/licenses"
            BASE "${android_openssl}"
            FILES "${android_openssl}/LICENSE"
        )
    endif()
    return()
endif()

# 解出的 GPU 画面直接画（gpu_frame_sources）
target_compile_definitions(Bilin PRIVATE RC_GPU_FRAMES)

# 电脑版只做 macOS 和 Windows
if(APPLE)
    target_sources(Bilin PRIVATE src/windowchrome_mac.mm src/dock_mac.mm src/login_item_mac.mm
                   src/quit_reason_mac.mm src/remote_view/frame_textures_mac.mm src/setup/setup_mac.mm)
    target_link_libraries(Bilin PRIVATE "-framework AppKit" "-framework CoreVideo" "-framework Metal"
                          "-framework ServiceManagement")
elseif(WIN32)
    target_sources(Bilin PRIVATE src/windowchrome.cpp src/dock.cpp src/login_item_win.cpp
                   src/quit_reason_win.cpp src/remote_view/frame_textures_win.cpp src/setup/setup_win.cpp
                   platform/windows/icon.rc)
    target_link_libraries(Bilin PRIVATE QWKQuick d3d11 advapi32 shell32 ole32)
else()
    message(FATAL_ERROR "界面程序只支持 macOS 和 Windows")
endif()

# 程序图标（App/branding，scripts/make-icons.sh 从 Logo 生成）：macOS 放进 .app 的 Resources；Windows 在 icon.rc 里
if(APPLE)
    set(app_icon "${CMAKE_CURRENT_SOURCE_DIR}/branding/Bilin.icns")
    target_sources(Bilin PRIVATE "${app_icon}")
    set_source_files_properties("${app_icon}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    # 访达、程序坞、菜单栏里显示"比邻"（Info.plist 里 LSHasLocalizedDisplayName，名字在 InfoPlist.strings）
    foreach(language zh-Hans en)
        set(strings "${CMAKE_CURRENT_SOURCE_DIR}/platform/macos/${language}.lproj/InfoPlist.strings")
        target_sources(Bilin PRIVATE "${strings}")
        set_source_files_properties("${strings}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/${language}.lproj")
    endforeach()
endif()

set_target_properties(Bilin PROPERTIES
    MACOSX_BUNDLE_ICON_FILE Bilin.icns
    MACOSX_BUNDLE_GUI_IDENTIFIER ${app_identifier}
    MACOSX_BUNDLE_BUNDLE_NAME Bilin
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/platform/macos/Info.plist.in"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# 后台服务和界面程序放在一起，界面从自己所在的地方启动它：Windows 上 Qt 把可执行文件都放在构建目录的顶层；
# macOS 上放进 .app 的 Contents/Helpers（rc/programs.h：不在 Contents/MacOS，Dock 里不多出图标）。单独一步：只重新编译了
# 后台服务时也要换进去。
# macOS 按代码签名记录"屏幕录制""辅助功能"的授权。开发时用 ad-hoc 签名，但把指定要求（designated requirement）
# 固定成标识：ad-hoc 签名默认的要求是代码的哈希，每次编译都变，授权就丢了。先签里面的后台服务和助手，再签整个包。
# 装好时录屏的是助手（rc/system_service.h），授权记在它名下
add_dependencies(Bilin rcservice)
if(APPLE)
    add_dependencies(Bilin rcagent)
    set(bundle_stamp "${CMAKE_CURRENT_BINARY_DIR}/bundle.stamp")
    add_custom_command(OUTPUT "${bundle_stamp}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:Bilin>/Helpers"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:rcservice>" "$<TARGET_FILE:rcagent>"
                "$<TARGET_BUNDLE_CONTENT_DIR:Bilin>/Helpers/"
        COMMAND codesign --force --sign - --identifier ${service_identifier}
                "-r=designated => identifier \"${service_identifier}\""
                "$<TARGET_BUNDLE_CONTENT_DIR:Bilin>/Helpers/$<TARGET_FILE_NAME:rcservice>"
        COMMAND codesign --force --sign - --identifier ${agent_identifier}
                "-r=designated => identifier \"${agent_identifier}\""
                "$<TARGET_BUNDLE_CONTENT_DIR:Bilin>/Helpers/$<TARGET_FILE_NAME:rcagent>"
        COMMAND codesign --force --sign - --identifier ${app_identifier}
                "-r=designated => identifier \"${app_identifier}\""
                "$<TARGET_BUNDLE_DIR:Bilin>"
        COMMAND ${CMAKE_COMMAND} -E touch "${bundle_stamp}"
        DEPENDS rcservice rcagent Bilin
        VERBATIM)
    add_custom_target(BilinBundle ALL DEPENDS "${bundle_stamp}")
endif()

install(TARGETS Bilin
    COMPONENT app
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
# 装的时候把 Qt 的库、插件、QML 模块放进程序旁边（Windows）/ 包里（macOS）。后台服务用到的（QtWebSockets
# 这些界面程序不用的）也要一起放：交给部署工具的不只界面程序，还有后台服务（macOS 还有助手）
if(APPLE)
    set(deploy_app "$<TARGET_FILE_NAME:Bilin>.app")
    set(deploy_helpers "\"${deploy_app}/Contents/Helpers/$<TARGET_FILE_NAME:rcservice>\" \"${deploy_app}/Contents/Helpers/$<TARGET_FILE_NAME:rcagent>\"")
    set(deploy_options "")
else()
    set(deploy_app "\${QT_DEPLOY_BIN_DIR}/$<TARGET_FILE_NAME:Bilin>")
    set(deploy_helpers "\"\${QT_DEPLOY_BIN_DIR}/$<TARGET_FILE_NAME:rcservice>\"")
    # VC 运行库的 DLL 放在程序旁边（下面），不要部署工具放的 vc_redist 安装器（没人运行它）
    set(deploy_options "DEPLOY_TOOL_OPTIONS --no-compiler-runtime")
    # 没装过 VC 运行库的电脑上也打得开（安装界面也是这个程序）。从 VS 开发环境（vcvars）给的 VCToolsRedistDir 里拿：
    # CMake 自带的 InstallRequiredSystemLibraries 还不认识 VS 18（v145）。通用 C 运行库（UCRT）Windows 10 起自带
    # 路径换成 CMake 的写法（/）：带着反斜杠写进安装脚本时会被当成转义
    file(TO_CMAKE_PATH "$ENV{VCToolsRedistDir}" vc_redist)
    file(GLOB vc_runtime "${vc_redist}/$ENV{VSCMD_ARG_TGT_ARCH}/Microsoft.VC*.CRT/*.dll")
    if(NOT vc_runtime)
        message(FATAL_ERROR "找不到 VC 运行库：要在 VS 的开发环境里配置（vcvars，scripts/build-windows.cmd）")
    endif()
    install(FILES ${vc_runtime} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT app)
endif()
# 用函数 qt6_：qt_generate_deploy_script 是宏，会把 CONTENT 里留给部署时的 ${…} 提前展开成空
qt6_generate_deploy_script(
    TARGET Bilin
    OUTPUT_SCRIPT deploy_script
    CONTENT "
qt6_deploy_qml_imports(TARGET Bilin PLUGINS_FOUND plugins_found)
qt6_deploy_runtime_dependencies(
    EXECUTABLE \"${deploy_app}\"
    ADDITIONAL_EXECUTABLES ${deploy_helpers}
    ADDITIONAL_MODULES \${plugins_found}
    ${deploy_options}
)
")
install(SCRIPT ${deploy_script} COMPONENT app)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
