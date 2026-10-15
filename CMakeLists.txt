cmake_minimum_required(VERSION 3.25)

# 最低系统，跟 SDK 一致：macOS 14（它要 ScreenCaptureKit），iOS 17（同一年的，VideoToolbox 选硬件解码器）。
# Qt 的 iOS 工具链到 project() 才说要编什么，所以按 iOS 的 sysroot 认（scripts/build-ios.sh）
if(CMAKE_OSX_SYSROOT MATCHES "^iphone")
    set(CMAKE_OSX_DEPLOYMENT_TARGET 17.0 CACHE STRING "Minimum iOS version")
elseif(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 14.0 CACHE STRING "Minimum macOS version")
endif()

project(Bilin VERSION 0.1.2 LANGUAGES C CXX)
# 版本的阶段：1.0 之前是 beta。界面、安装信息、发布的新版本都写成 0.1.0-beta；比新旧只看数字，所以每次发布数字都要变大。
# macOS 包里的版本号只能是数字（PROJECT_VERSION）
set(BILIN_VERSION_LABEL beta)
if(BILIN_VERSION_LABEL)
    set(BILIN_VERSION "${PROJECT_VERSION}-${BILIN_VERSION_LABEL}")
else()
    set(BILIN_VERSION "${PROJECT_VERSION}")
endif()
# 语言要在顶层启用，子目录才能用
if(APPLE)
    enable_language(OBJCXX)
endif()

# 手机版（Android、iOS）：只去连电脑，不被连（App/CMakeLists.txt）。电脑上也能编手机版，在窗口里预览手机的界面
option(BILIN_MOBILE "手机版" OFF)
if(ANDROID OR IOS)
    set(BILIN_MOBILE ON CACHE BOOL "手机版" FORCE)
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# 源文件都是 UTF-8（字符串里有中文）
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

# Windows 上程序放在安装目录的最上层，Qt 的库和插件挨着它。要在 find_package(Qt6) 之前：
# Qt 在那时按 GNUInstallDirs 定下部署的目录
if(WIN32)
    set(CMAKE_INSTALL_BINDIR .)
endif()
find_package(Qt6 6.10 REQUIRED COMPONENTS Core)
qt_standard_project_setup(REQUIRES 6.10)

include(CTest)

# 服务器的长连接地址。开发时服务端在 Mac 上的 OrbStack 虚拟机里（Server/deploy），Mac 上是 localhost；
# 局域网里别的电脑写 Mac 的地址，比如 ws://192.168.1.10:8800/ws。安装包连正式的服务器 wss://www.bilindesk.com/ws
# （scripts/build-macos-pkg.sh、build-windows-installer.cmd），证书按系统信任的证书机构校验
set(RC_SERVER_URL "ws://localhost:8800/ws" CACHE STRING "服务器的长连接地址")
# 程序核对新版本的安装包用的发布公钥（rc/release.h，docs/admin.md）：服务器用的同一个文件，scripts/sign-release.mjs 生成
file(READ "${CMAKE_SOURCE_DIR}/Server/release-key.pub" RC_RELEASE_KEY)
string(STRIP "${RC_RELEASE_KEY}" RC_RELEASE_KEY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/Server/release-key.pub")
# 程序用的 SDK 是哪个版本（它的头文件里定义的，比如 0.1.0-beta）：两台机器连接时核对（rc/transport.h）
set(rcsdk_header "${CMAKE_SOURCE_DIR}/third_party/rcsdk/include/rcsdk/rcsdk.h")
if(NOT EXISTS "${rcsdk_header}")
    message(FATAL_ERROR "没有 SDK：third_party/rcsdk 是子模块，先 git submodule update --init")
endif()
foreach(part MAJOR MINOR PATCH)
    file(STRINGS "${rcsdk_header}" line REGEX "^#define RCSDK_VERSION_${part} [0-9]+u$")
    string(REGEX REPLACE "^#define RCSDK_VERSION_${part} ([0-9]+)u$" "\\1" rcsdk_${part} "${line}")
endforeach()
file(STRINGS "${rcsdk_header}" line REGEX "^#define RCSDK_VERSION_LABEL \"[a-z]*\"$")
string(REGEX REPLACE "^#define RCSDK_VERSION_LABEL \"([a-z]*)\"$" "\\1" rcsdk_label "${line}")
set(RC_SDK_VERSION "${rcsdk_MAJOR}.${rcsdk_MINOR}.${rcsdk_PATCH}")
if(rcsdk_label)
    string(APPEND RC_SDK_VERSION "-${rcsdk_label}")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${rcsdk_header}")

# Android 上的 OpenSSL：Qt 的加密连接（连正式服务器的 wss://）要它，Qt 不带；核对新版本安装包的签名（rc/release.h）
# 也用它。用 Android SDK 目录里 KDAB 的 android_openssl（Qt 的文档推荐，Qt Creator 装 Android 开发环境时一起下好），
# 打进 APK（App/CMakeLists.txt）
if(ANDROID)
    set(android_openssl "${ANDROID_SDK_ROOT}/android_openssl")
    set(android_openssl_abi "${android_openssl}/ssl_3/${CMAKE_ANDROID_ARCH_ABI}")
    if(NOT EXISTS "${android_openssl_abi}/libssl_3.so")
        message(FATAL_ERROR "没有 Android 的 OpenSSL（${android_openssl_abi}）：在 Qt Creator 的 Android 设置里装，"
                            "或者把 https://github.com/KDAB/android_openssl 克隆到 Android SDK 目录下")
    endif()
    add_library(android_openssl_crypto SHARED IMPORTED)
    set_target_properties(android_openssl_crypto PROPERTIES
        IMPORTED_LOCATION "${android_openssl_abi}/libcrypto_3.so"
        INTERFACE_INCLUDE_DIRECTORIES "${android_openssl_abi}/include")
endif()

# 远程控制 SDK（子模块，third_party/README.md）以源码编进程序：静态链接，头文件和库必定同一版本，也不用另外分发动态库
set(BUILD_SHARED_LIBS OFF)
# SDK 自带的软件编解码库（VP8/VP9、AV1）不编进来：用 SDK 默认的 H.264，系统的编解码器就够；带上要大约 7 MB，
# Windows 上还要另装 NASM
set(RCSDK_WITH_LIBVPX OFF)
set(RCSDK_WITH_DAV1D OFF)
set(RCSDK_WITH_LIBAOM OFF)
add_subdirectory(third_party/rcsdk EXCLUDE_FROM_ALL)

# Windows 的窗口外框：系统边框、阴影、贴靠布局保留，标题栏和窗口按钮由程序画
if(WIN32)
    set(QWINDOWKIT_BUILD_STATIC ON)
    set(QWINDOWKIT_BUILD_WIDGETS OFF)
    set(QWINDOWKIT_BUILD_QUICK ON)
    set(QWINDOWKIT_INSTALL OFF)
    set(QWINDOWKIT_ENABLE_STYLE_AGENT OFF)
    add_subdirectory(third_party/qwindowkit EXCLUDE_FROM_ALL)
endif()

# macOS 的包标识；后台服务是它下面的 .service，装好时各个会话里的助手是 .agent（代码签名、Info.plist 都用）
set(app_identifier com.bilindesk.bilin)
set(service_identifier ${app_identifier}.service)
set(agent_identifier ${app_identifier}.agent)

add_subdirectory(Core)
add_subdirectory(Service)
add_subdirectory(App)
if(NOT BILIN_MOBILE)
    add_subdirectory(tools)
endif()
