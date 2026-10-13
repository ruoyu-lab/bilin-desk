# 开发、测试用的小工具，不装进程序里
add_subdirectory(rcconnect)
if(WIN32)
    add_subdirectory(setup)
endif()
