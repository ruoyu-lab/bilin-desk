find_package(Qt6 REQUIRED COMPONENTS Gui Qml QuickTest Test)

# 开机时自动启动：系统的登录项（login_item.h）换成测试里假的，不碰真的
qt_add_executable(tst_autostart tst_autostart.cpp ../src/autostart.h ../src/autostart.cpp ../src/login_item.h)
target_include_directories(tst_autostart PRIVATE ../src)
target_link_libraries(tst_autostart PRIVATE Qt6::Gui Qt6::Qml Qt6::Test)
add_test(NAME tst_autostart COMMAND tst_autostart)
# 不连窗口服务器
set_tests_properties(tst_autostart PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)

# 安装、卸载一步一步做，没成就撤回
qt_add_executable(tst_setup_steps tst_setup_steps.cpp ../src/setup/setup_steps.h ../src/setup/setup_steps.cpp)
target_include_directories(tst_setup_steps PRIVATE ../src)
target_link_libraries(tst_setup_steps PRIVATE Qt6::Core Qt6::Test)
add_test(NAME tst_setup_steps COMMAND tst_setup_steps)

# 和一台设备的对话：存下来、没看的几条、存不下时的说明
qt_add_executable(tst_conversation tst_conversation.cpp ../src/chat/conversation.h ../src/chat/conversation.cpp)
target_include_directories(tst_conversation PRIVATE ../src)
target_link_libraries(tst_conversation PRIVATE rccore Qt6::Qml Qt6::Test)
add_test(NAME tst_conversation COMMAND tst_conversation)

# 互传文件的列表：给人看的说明，收下时存到哪里（接着上次的存回上次的文件夹）
qt_add_executable(tst_file_transfer_list tst_file_transfer_list.cpp ../src/files/file_transfer_list.h
                  ../src/files/file_transfer_list.cpp)
target_include_directories(tst_file_transfer_list PRIVATE ../src)
target_link_libraries(tst_file_transfer_list PRIVATE rccore Qt6::Qml Qt6::Test)
add_test(NAME tst_file_transfer_list COMMAND tst_file_transfer_list)

# 远程声音听不到时的说明（对方录不了的）
qt_add_executable(tst_viewer_audio tst_viewer_audio.cpp ../src/session/viewer_audio.h ../src/session/viewer_audio.cpp)
target_include_directories(tst_viewer_audio PRIVATE ../src)
target_link_libraries(tst_viewer_audio PRIVATE rccore RemoteControl::Core Qt6::Test)
add_test(NAME tst_viewer_audio COMMAND tst_viewer_audio)

# 会话里的分辨率、帧率：选项不超过对方显示器，要求里的尺寸、帧率、码率
qt_add_executable(tst_view_quality tst_view_quality.cpp ../src/session/view_quality.h ../src/session/view_quality.cpp)
target_include_directories(tst_view_quality PRIVATE ../src)
target_link_libraries(tst_view_quality PRIVATE RemoteControl::Core Qt6::Test)
add_test(NAME tst_view_quality COMMAND tst_view_quality)

# 设备列表的筛选和排序（qml/main/device_query.js；测试在 qml/ 里）
qt_add_executable(tst_device_query tst_device_query.cpp)
target_compile_definitions(tst_device_query PRIVATE QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")
target_link_libraries(tst_device_query PRIVATE Qt6::Qml Qt6::QuickTest)
add_test(NAME tst_device_query COMMAND tst_device_query)
set_tests_properties(tst_device_query PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
