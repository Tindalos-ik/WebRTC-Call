cmake_minimum_required(VERSION 3.16)

project(WebRTCCall LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_EXTENSIONS OFF)
# 生成 compile_commands.json，供 clangd、ccls 等工具做代码补全和跳转
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 如果用户没有指定构建类型，默认使用 Release，获得更好的运行时性能
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# 统一可执行文件和静态库的输出目录，方便查找产物
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# WebRTC 依赖路径可通过 -D<name>=<path> 覆盖
# WebRTC M94 源码目录，默认在用户主目录下的 webrtc-checkout/src
set(WEBRTC_ROOT "$ENV{HOME}/webrtc-checkout/src" CACHE PATH "WebRTC M94 source directory")
# WebRTC M94 的构建输出目录，里面应有 obj/libwebrtc.a
set(WEBRTC_OUT "${WEBRTC_ROOT}/out/Release" CACHE PATH "WebRTC M94 build directory")
# libhv 使用项目 third_party 目录中的头文件和动态库
set(LIBHV_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libhv")

# 查找依赖：线程库、X11、SDL2
find_package(Threads REQUIRED)
find_package(X11 REQUIRED)
find_package(SDL2 REQUIRED CONFIG)

# 检查 WebRTC 静态库是否存在，不存在则直接报错并提示如何设置路径
# 这里的路径对应 M94 使用 use_custom_libcxx=false 和 use_sysroot=false 编译出的产物
if(NOT EXISTS "${WEBRTC_OUT}/obj/libwebrtc.a")
    message(FATAL_ERROR "WebRTC library not found. Set WEBRTC_ROOT and WEBRTC_OUT to your M94 build.")
endif()

# 将 WebRTC 静态库声明为 IMPORTED 目标，CMake 不会去编译它，只负责链接
add_library(webrtc_m94 STATIC IMPORTED GLOBAL)
# 起个别名 WebRTC::WebRTC，后续链接时更直观
add_library(WebRTC::WebRTC ALIAS webrtc_m94)
set_target_properties(webrtc_m94 PROPERTIES
    # 静态库的实际路径
    IMPORTED_LOCATION "${WEBRTC_OUT}/obj/libwebrtc.a"
    # 头文件搜索路径：WebRTC 源码根目录、abseil、libyuv
    INTERFACE_INCLUDE_DIRECTORIES "${WEBRTC_ROOT};${WEBRTC_ROOT}/third_party/abseil-cpp;${WEBRTC_ROOT}/third_party/libyuv/include"
    # 编译宏定义：表明是 POSIX/Linux 平台，且为 Release 模式
    INTERFACE_COMPILE_DEFINITIONS "WEBRTC_POSIX;WEBRTC_LINUX;NDEBUG"
    # 要求使用 C++14
    INTERFACE_COMPILE_FEATURES cxx_std_14
    # 与 WebRTC M94 的 RTTI 编译选项保持一致
    INTERFACE_COMPILE_OPTIONS "-fno-rtti"
    # 链接 WebRTC 所需的系统库：线程、原子、dl、数学库、X11 系列
    INTERFACE_LINK_LIBRARIES "Threads::Threads;atomic;${CMAKE_DL_LIBS};m;rt;X11::X11;X11::Xext;X11::Xrandr;X11::Xcomposite;X11::Xdamage;X11::Xfixes;X11::Xrender;X11::Xtst"
)

# 项目内的 libhv 头文件目录和动态库文件
set(LIBHV_INCLUDE_DIR "${LIBHV_ROOT}/include")
set(LIBHV_LIBRARY "${LIBHV_ROOT}/lib/libhv.so")
# 检查复制到项目中的文件
if(NOT EXISTS "${LIBHV_INCLUDE_DIR}/hv/HttpClient.h" OR NOT EXISTS "${LIBHV_LIBRARY}")
    message(FATAL_ERROR "libhv files not found under ${LIBHV_ROOT}.")
endif()

# 将项目内的 libhv.so 声明为共享库目标
add_library(libhv SHARED IMPORTED GLOBAL)
# 起别名 libhv::hv，方便链接
add_library(libhv::hv ALIAS libhv)
set_target_properties(libhv PROPERTIES
    # 库文件的实际路径
    IMPORTED_LOCATION "${LIBHV_LIBRARY}"
    # 头文件搜索路径
    INTERFACE_INCLUDE_DIRECTORIES "${LIBHV_INCLUDE_DIR}"
    # 链接 libhv 所需的系统库：线程和 dl
    INTERFACE_LINK_LIBRARIES "Threads::Threads;${CMAKE_DL_LIBS}"
)

# 将 pusher 和 puller 共用的实现代码编译成一个静态库 client
# 这样两个可执行文件都能复用，避免重复编译
add_library(client STATIC
    src/client/webrtc_client.cpp
    src/client/webrtc_stream.cpp
    src/client/signal_peer_client.cpp
    src/client/vcm_capture.cpp
    src/client/cam_video_source.cpp
    src/client/video_renderer.cpp
)
# 项目头文件供调用方使用，JSON 单头文件用于 client 内部实现
target_include_directories(client
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/json/include"
)
# client 库依赖 WebRTC、libhv、SDL2
target_link_libraries(client PUBLIC WebRTC::WebRTC libhv::hv SDL2::SDL2)

# 循环生成两个可执行文件：pusher（推流端）和 puller（拉流端）
# 它们都链接同一个 client 静态库
foreach(app IN ITEMS pusher puller)
    add_executable(${app} "src/${app}.cpp")
    target_link_libraries(${app} PRIVATE client)
endforeach()