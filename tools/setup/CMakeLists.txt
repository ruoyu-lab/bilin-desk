# Windows 的安装程序（不用 Qt）：stub 是 Bilin-Setup.exe 的程序部分（解压、以安装模式启动界面程序），
# pack 把程序文件接到它后面（scripts\build-windows-installer.cmd）。格式在 archive.h
# 安装程序的图标和程序的一样（App/platform/windows/icon.rc）
add_executable(BilinSetup WIN32 archive.h stub.cpp setup.manifest ${CMAKE_SOURCE_DIR}/App/platform/windows/icon.rc)
target_link_libraries(BilinSetup PRIVATE Cabinet shell32)
# 要管理员身份写在 setup.manifest 里：链接器自己不再加一份
target_link_options(BilinSetup PRIVATE /MANIFESTUAC:NO)
# 静态链接 C 运行库：在还没装 VC 运行库的电脑上也要能跑
set_property(TARGET BilinSetup PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(BilinPack archive.h pack.cpp)
target_link_libraries(BilinPack PRIVATE Cabinet)

set_target_properties(BilinSetup BilinPack PROPERTIES AUTOMOC OFF)
