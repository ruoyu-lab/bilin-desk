# rcconnect：命令行去连一台设备，打印连接的每一步（测试、排查用）
# 用后台服务的代码（rcservicecore），它的 Qt 依赖在这里也要找得到
find_package(Qt6 REQUIRED COMPONENTS Core Network WebSockets)

qt_add_executable(rcconnect main.cpp)
target_link_libraries(rcconnect PRIVATE rcservicecore rcfiles RemoteControl::Core RemoteControl::FileTransfer)
target_compile_definitions(rcconnect PRIVATE RC_SERVER_URL="${RC_SERVER_URL}")
