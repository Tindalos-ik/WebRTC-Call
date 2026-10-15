# WebRTC-Call

基于 **WebRTC M94 C++ Native** 的音视频推拉流项目，使用 **ZLMediaKit** 处理 HTTP 信令和音视频转发，使用 **SDL2** 显示接收到的视频。

项目提供两个程序：

- `pusher`：采集本地麦克风和摄像头，向 ZLMediaKit 发起音视频推流。
- `puller`：从 ZLMediaKit 拉取音视频，在 SDL 窗口中显示视频。

## 环境与依赖

当前开发环境为 **Ubuntu 22.04 / WSL2**，使用 C++14 和 CMake。

| 依赖 | 用途 | 来源 |
| --- | --- | --- |
| WebRTC M94 | 连接协商、音视频采集、编解码和传输 | 本机编译的源码和 `libwebrtc.a` |
| ZLMediaKit | 交换 SDP，接收和转发音视频流 | 独立运行的服务器 |
| libhv | 发送异步 HTTP 信令请求 | `third_party/libhv/` |
| nlohmann/json 3.11.3 | 解析服务器返回的 JSON | `third_party/json/include/json.hpp` |
| SDL2 | 创建窗口和显示视频 | Ubuntu 系统依赖 |
| X11 开发库 | 满足 WebRTC 的链接依赖 | Ubuntu 系统依赖 |

安装构建工具和系统开发库：

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake ninja-build pkg-config libsdl2-dev \
  libx11-dev libxext-dev libxrandr-dev libxcomposite-dev \
  libxdamage-dev libxfixes-dev libxrender-dev libxtst-dev
```

### WebRTC M94

先准备编译完成的 WebRTC M94。默认目录为：

```text
~/webrtc-checkout/src/
└── out/Release/obj/libwebrtc.a
```

项目使用系统 `libstdc++`，对应 WebRTC 构建选项 `use_custom_libcxx=false`、`use_sysroot=false`。CMake 已为使用 WebRTC 的目标配置 `-fno-rtti`，与当前 M94 库保持一致。

### ZLMediaKit

运行启用 WebRTC 支持的 [ZLMediaKit](https://github.com/ZLMediaKit/ZLMediaKit)。本文示例使用以下端口：

- HTTP：`8080`，用于提交 Offer、获取 Answer。
- WebRTC：`8000`，用于音视频传输，需放通 UDP 和 TCP。

跨机器使用时，将示例 URL 中的 `127.0.0.1` 改为服务器地址，并将 ZLMediaKit 的 `rtc.externIP` 配置为客户端能够访问的服务器地址。

## 目录结构

```text
WebRTC-Call/
├── CMakeLists.txt          # 根目录统一管理构建
├── include/client/        # 公共头文件
├── src/
│   ├── client/            # 会话、信令、采集和显示实现
│   ├── pusher.cpp         # 推流程序入口
│   └── puller.cpp         # 拉流程序入口
├── third_party/
│   ├── libhv/
│   │   ├── include/hv/    # libhv 头文件
│   │   └── lib/libhv.so   # 编译好的动态库
│   └── json/include/json.hpp
├── .vscode/               # VS Code C++ 配置
└── .gitignore
```

核心类：

| 类 | 作用 |
| --- | --- |
| `WebRTCClient` | 提供推拉流入口，在管理线程中创建和关闭会话，按 URL 保存会话 |
| `WebRTCStream` | 管理一条 WebRTC 会话，创建连接、生成 Offer、处理 Answer 和远端轨道 |
| `SignalPeerClient` | 使用 libhv 向 ZLMediaKit 发送 SDP，解析响应并返回 Answer |
| `VideoRenderer` | 接收解码后的视频帧，交给 SDL 主线程绘制，按视频尺寸创建纹理 |

## 编译

在项目根目录执行：

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
```

WebRTC 安装在其他目录时，通过参数指定路径：

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DWEBRTC_ROOT="$HOME/webrtc-checkout/src" \
  -DWEBRTC_OUT="$HOME/webrtc-checkout/src/out/Release"
cmake --build build -j2
```

构建结果：

- `build/bin/pusher`：推流程序。
- `build/bin/puller`：拉流程序。
- `build/lib/libclient.a`：两个程序共用的客户端实现。
- `build/compile_commands.json`：供编辑器读取的编译配置。

## 运行

先启动 ZLMediaKit。推流程序需要 Ubuntu 能访问到麦克风和摄像头，拉流程序需要可显示 SDL 窗口的图形环境；WSL2 可以使用 WSLg。

### 推流

```bash
./build/bin/pusher "http://127.0.0.1:8080/index/api/webrtc?app=live&stream=test&type=push"
```

### 拉流

确认服务器已有对应的流，再运行：

```bash
./build/bin/puller "http://127.0.0.1:8080/index/api/webrtc?app=live&stream=test&type=play"
```

URL 中，`app=live` 表示应用名，`stream=test` 表示流名，`type` 选择推流或拉流。推拉同一路视频时，使用相同的 `app` 和 `stream`。

URL 包含 `&`，运行时保留外层引号。

如果 WSL 暂无摄像头，可以先用浏览器或其他推流端向 ZLMediaKit 提供视频，再运行 `puller` 验证拉流。

推流程序通过 Ctrl+C 退出；拉流程序支持关闭窗口或 Ctrl+C 退出。

## 参考

- [WebRTC C++ 示例项目](https://gitee.com/mengbieting_0/webrtc_cpp_demo_code)
- [ZLMediaKit](https://github.com/ZLMediaKit/ZLMediaKit)
