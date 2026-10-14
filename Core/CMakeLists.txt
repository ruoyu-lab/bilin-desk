# 界面程序和后台服务共用的部分，没有界面：本机身份、两者之间的本机连接和消息、和服务器及局域网里别的电脑之间的消息、数据放在哪里
find_package(Qt6 REQUIRED COMPONENTS Core Network)

qt_add_library(rccore STATIC
    src/rc/chat_history.h
    src/rc/chat_history.cpp
    src/rc/chat_protocol.h
    src/rc/chat_protocol.cpp
    src/rc/data_channels.h
    src/rc/data_channels.cpp
    src/rc/device_id.h
    src/rc/device_id.cpp
    src/rc/device_name.h
    src/rc/download_mark.h
    src/rc/control_protocol.h
    src/rc/control_protocol.cpp
    src/rc/interrupted_transfers.h
    src/rc/interrupted_transfers.cpp
    src/rc/json_file.h
    src/rc/json_file.cpp
    src/rc/lan_protocol.h
    src/rc/lan_protocol.cpp
    src/rc/line_channel.h
    src/rc/line_channel.cpp
    src/rc/log_file.h
    src/rc/log_file.cpp
    src/rc/machine.h
    src/rc/machine.cpp
    src/rc/network_path.h
    src/rc/network_path.cpp
    src/rc/paths.h
    src/rc/paths.cpp
    src/rc/peer_protocol.h
    src/rc/power_actions.h
    src/rc/programs.h
    src/rc/protocol.h
    src/rc/protocol.cpp
    src/rc/release.h
    src/rc/release.cpp
    src/rc/server_protocol.h
    src/rc/server_protocol.cpp
    src/rc/system_service.h
    src/rc/temporary_password.h
    src/rc/temporary_password.cpp
    src/rc/transfer_list.h
    src/rc/transfer_list.cpp
    src/rc/transport.h
    src/rc/transport.cpp
    src/rc/user_seal.h
    src/rc/ipc/endpoint.h
    src/rc/ipc/endpoint.cpp
)

if(APPLE)
    target_sources(rccore PRIVATE src/rc/device_name_mac.cpp src/rc/download_mark_mac.mm src/rc/user_seal_plain.cpp
                   src/rc/system_service_mac.cpp src/rc/release_mac.cpp src/rc/machine_mac.cpp)
    target_link_libraries(rccore PRIVATE "-framework CoreFoundation" "-framework SystemConfiguration"
                          "-framework Foundation" "-framework CoreServices" "-framework Security" "-framework IOKit")
elseif(WIN32)
    target_sources(rccore PRIVATE src/rc/device_name.cpp src/rc/download_mark_win.cpp src/rc/user_seal_win.cpp
                   src/rc/system_service_win.cpp src/rc/release_win.cpp src/rc/machine_win.cpp)
    target_link_libraries(rccore PRIVATE crypt32 advapi32 shell32 ole32 bcrypt)
elseif(ANDROID)
    # 手机上没有系统服务（rc/system_service.h）；核对新版本的签名用 OpenSSL（顶层 CMakeLists.txt）
    target_sources(rccore PRIVATE src/rc/device_name_android.cpp src/rc/download_mark_android.cpp
                   src/rc/user_seal_plain.cpp src/rc/machine_android.cpp src/rc/release_android.cpp)
    target_link_libraries(rccore PRIVATE android_openssl_crypto)
endif()

target_include_directories(rccore PUBLIC src)
# 发布公钥（rc/release.h）：和服务器用的同一个文件
target_compile_definitions(rccore PRIVATE RC_RELEASE_KEY="${RC_RELEASE_KEY}" RC_SDK_VERSION="${RC_SDK_VERSION}")
target_link_libraries(rccore PUBLIC Qt6::Core Qt6::Network)

# 互传文件的 SDK 那一层（SDK 的文件传输模块），界面程序（去连别人）和后台服务（别人连本机）共用。
# 单独一个库：rccore 和它的测试不链接 SDK
qt_add_library(rcfiles STATIC
    src/rc/file_transfers.h
    src/rc/file_transfers.cpp
)
target_link_libraries(rcfiles PUBLIC rccore RemoteControl::FileTransfer)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
