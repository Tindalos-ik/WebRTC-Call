# WebRTC-Call

使用WebRTC C++ Native 实现



## 架构设计

参考 [webrtc_cpp_demo_code](https://gitee.com/mengbieting_0/webrtc_cpp_demo_code)。本项目使用 **ZLMediaKit** 接收和转发音视频，整体结构为「推流端 → ZLMediaKit → 拉流端」。

![WebRTC 推拉流架构图](assets/webrtc-native-architecture.png)

虚线表示信令交互，实线表示模块调用或媒体流向。



## 环境搭建

### WebRTC C++ Native

官方没有提供可直接使用的 C++ Native 安装包，需要从源码构建。这里选择 **M94 版本**，使用 GN + Ninja 编译，再通过 CMake 导入项目。

先安装基础工具和 Python 环境：

```bash
sudo apt update
sudo apt install -y git python3 python-is-python3 curl wget pkg-config \
  ninja-build build-essential cmake ca-certificates
```

下载源码需要代理，本地 Clash 端口是 `7897`。在 `~/.bashrc` 中添加：

```bash
export http_proxy="http://127.0.0.1:7897"
export https_proxy="$http_proxy"
export HTTP_PROXY="$http_proxy"
export HTTPS_PROXY="$http_proxy"
unset ALL_PROXY all_proxy
export no_proxy="localhost,127.0.0.1,::1"
export NO_PROXY="$no_proxy"
```

使配置生效，同时给 Git 配置代理：

```bash
source ~/.bashrc
git config --global http.proxy http://127.0.0.1:7897
git config --global https.proxy http://127.0.0.1:7897
```

然后安装用于下载和管理 WebRTC 源码的 `depot_tools`：

```bash
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git ~/depot_tools

echo 'export PATH="$HOME/depot_tools:$PATH"' >> ~/.bashrc
echo 'export DEPOT_TOOLS_UPDATE=0' >> ~/.bashrc
source ~/.bashrc

gclient --help
```

安装编译所需的音频、图形等开发库：

```bash
sudo apt install -y \
  libasound2-dev libpulse-dev libudev-dev \
  libx11-dev libxext-dev libxrandr-dev libxcomposite-dev \
  libxdamage-dev libxfixes-dev libxtst-dev libxi-dev libxrender-dev \
  libglib2.0-dev libdrm-dev libgbm-dev libgtk-3-dev nasm yasm
```

接下来下载 M94 源码及其依赖。下面的提交对应 M94 的 `branch-heads/4606`：

```bash
mkdir -p ~/webrtc-checkout
cd ~/webrtc-checkout

gclient config \
  https://webrtc.googlesource.com/src.git@6584528aeb0f0e2ab4d14114aefeee7e5997ade9

gclient sync --no-history --nohooks -j 4 \
  --revision src@6584528aeb0f0e2ab4d14114aefeee7e5997ade9
```

源码下载到 `~/webrtc-checkout/src`。进入源码目录，下载配套编译器并生成构建信息：

```bash
cd ~/webrtc-checkout/src
python3 tools/clang/scripts/update.py
python3 build/util/lastchange.py -o build/util/LASTCHANGE
```

生成 Release 构建配置，再编译 WebRTC：

```bash
buildtools/linux64/gn gen out/Release --args='
is_debug=false
is_component_build=false
target_cpu="x64"
rtc_include_tests=false
rtc_build_examples=false
rtc_build_tools=false
use_custom_libcxx=false
use_sysroot=false
use_thin_lto=false
symbol_level=0
treat_warnings_as_errors=false
'

ninja -C out/Release webrtc -j2
```

这里关闭测试和示例，只编译库；`use_custom_libcxx=false` 让 WebRTC 使用系统 C++ 标准库，方便后面用 G++ 和 CMake 链接。

编译完成后得到：

```text
~/webrtc-checkout/src/out/Release/obj/libwebrtc.a
```

### ZLMediaKit 部署

在 Ubuntu 里运行 ZLMediaKit，用它接收发送端传来的画面，再转发给观看端。本机已经完成安装和启动，下面记录具体步骤。

#### 1. 安装编译工具和依赖库

```bash
sudo apt install -y build-essential cmake ninja-build git pkg-config \
  libssl-dev libsrtp2-dev
```

#### 2. 下载并编译服务器程序

把源码下载到 `~/ZLMediaKit`，再编译出要运行的 `MediaServer`：

```bash
git clone --depth=1 --recurse-submodules --shallow-submodules \
  https://github.com/ZLMediaKit/ZLMediaKit.git ~/ZLMediaKit
cd ~/ZLMediaKit

cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_WEBRTC=ON \
  -DENABLE_TESTS=OFF \
  -DENABLE_PLAYER=OFF \
  -DDISABLE_REPORT=ON

cmake --build build --target MediaServer -j2
```

#### 3. 配置服务器

在 `~/ZLMediaKit` 下创建 `config.local.ini`，填写下面的配置：

```ini
[api]
apiDebug=0

[general]
listen_ip=127.0.0.1

[http]
port=8080
sslport=0

[rtc]
externIP=127.0.0.1
port=8000
tcpPort=8000

[hook]
enable=0

[rtsp]
port=0
sslport=0

[rtmp]
port=0
sslport=0

[rtp_proxy]
port=0

[srt]
port=0

[shell]
port=0
```

`127.0.0.1` 表示这台电脑；`8080` 用来接收客户端的连接请求，`8000` 用来传输音视频。其他协议的端口设为 `0`，表示关闭。

#### 4. 启动服务器

运行下面的命令。`-c` 指定配置文件，`-t 2` 设置两个事件处理线程：

```bash
cd ~/ZLMediaKit
./release/linux/Release/MediaServer -c "$PWD/config.local.ini" -t 2
```

保持这个终端运行，服务器就能继续提供服务。按 `Ctrl+C` 可以停止；修改配置后，先停止再重新启动。

启动后，在浏览器打开它自带的测试网页：

```text
http://127.0.0.1:8080/webrtc/
```

Ubuntu 重启后，重新执行第 4 步的启动命令即可。





## ZLMediaKit

在这个项目中，**ZLMediaKit 是负责接收和转发音视频的服务器程序**，运行时的程序名叫 `MediaServer`。

### 推流和拉流是什么意思

把摄像头采集到的画面、麦克风采集到的声音发送给服务器，叫做**推流**。

从服务器接收画面和声音，再播放出来，叫做**拉流**。

项目中的 `pusher` 负责推流，`puller` 负责拉流。ZLMediaKit 在中间接收、转发，让观看端能看到发送端的画面。

### URL 告诉服务器要做什么

推流程序使用这个地址，向服务器发布一条叫 `test` 的流：

```text
http://127.0.0.1:8080/index/api/webrtc?app=live&stream=test&type=push
```

拉流程序使用这个地址，观看同一条流：

```text
http://127.0.0.1:8080/index/api/webrtc?app=live&stream=test&type=play
```

可以把 URL 拆开来看：

| 部分 | 在这里的含义 |
| --- | --- |
| `127.0.0.1` | 服务器运行在这台电脑上 |
| `8080` | 服务器接收 HTTP 请求的端口 |
| `/index/api/webrtc` | 找到 ZLMediaKit 处理 WebRTC 连接请求的入口 |
| `app=live` | 这条流属于 `live` 分组 |
| `stream=test` | 这条流的名字叫 `test` |
| `type=push` | 我要向服务器发送这条流 |
| `type=play` | 我要从服务器观看这条流 |

`app=live&stream=test` 组合起来，就选中了 `live/test` 这条流。多个观看端可以使用相同的拉流地址观看它。

如果再发送另一台摄像头的画面，可以把它命名为 `camera2`，使用 `stream=camera2`。`WebRtcClient` 会用完整 URL 找到对应的 `WebRTCStream` 会话。

### 客户端怎样连上服务器

开始传输前，双方先交换“怎样连接、采用什么音视频参数”的信息，这些信息叫做**信令**。

以推流为例：

1. `pusher` 向上面的推流 URL 发请求，表示“我要发布 `live/test`”，并带上自己生成的连接参数，称为 **Offer**。
2. ZLMediaKit 收到请求，返回自己这边的连接参数，称为 **Answer**。
3. 客户端把 Answer 交给 WebRTC，WebRTC 根据双方的参数建立连接。
4. 连接建立后，摄像头画面和麦克风声音持续发往服务器。

拉流时，`puller` 用 `type=play` 发起同样的连接过程，连接建立后从服务器接收音视频并播放。

在代码中，`SignalPeerClient` 负责通过 HTTP 发送和接收这些连接参数，`WebRTCStream` 负责把参数交给 WebRTC，并管理这条音视频会话。本机配置的 `8080` 处理连接请求，`8000` 承担后续的音视频传输。



### P2P 信令服务器

ZLMediaKit 还提供基于 **WebSocket 的 P2P 信令功能**，可以帮助两个客户端交换建立连接所需的信息。后续实现两端直接通话时，可以采用这条路线。

大致过程：

1. 两个客户端通过 WebSocket 连接 ZLMediaKit，注册各自的标识。
2. 发起通话的一端生成 Offer，由 ZLMediaKit 转发给对方。
3. 对方生成 Answer，经 ZLMediaKit 返回；双方还会交换可用于连接的候选网络地址，叫做 **ICE Candidate**。
4. 两端的 WebRTC 根据这些信息尝试建立直连。直连成功后，画面和声音就在两端之间传输。

后续可以开启服务器的 `rtc.signalingPort`，再给 `SignalPeerClient` 增加 WebSocket 消息的发送和接收，让它完成双方连接参数的交换。

这部分作为后续 P2P 通信的实现方向；本机当前运行的是前面介绍的推流、转发和拉流流程。

参考：[ZLMediaKit 官方 P2P 使用说明](https://github.com/ZLMediaKit/ZLMediaKit/blob/master/webrtc/USAGE.md)。




## 在 CMake 项目中使用

WebRTC 不自带可直接使用的 CMake 包，需要手动指定静态库、头文件和系统依赖。下面配置对应上面编译出的 M94 Release 库：

```cmake
cmake_minimum_required(VERSION 3.16)
project(webrtc_m94_demo LANGUAGES CXX)

set(WEBRTC_ROOT "$ENV{HOME}/webrtc-checkout/src" CACHE PATH "WebRTC source directory")
set(WEBRTC_OUT "${WEBRTC_ROOT}/out/Release" CACHE PATH "WebRTC build directory")

find_package(Threads REQUIRED)
find_package(X11 REQUIRED)

add_library(webrtc_m94 STATIC IMPORTED)
add_library(WebRTC::WebRTC ALIAS webrtc_m94)
set_target_properties(webrtc_m94 PROPERTIES
    IMPORTED_LOCATION "${WEBRTC_OUT}/obj/libwebrtc.a"
    INTERFACE_INCLUDE_DIRECTORIES "${WEBRTC_ROOT};${WEBRTC_ROOT}/third_party/abseil-cpp;${WEBRTC_ROOT}/third_party/libyuv/include"
    INTERFACE_COMPILE_DEFINITIONS "WEBRTC_POSIX;WEBRTC_LINUX;NDEBUG"
    INTERFACE_COMPILE_FEATURES cxx_std_14
    INTERFACE_COMPILE_OPTIONS "-fno-rtti"
    INTERFACE_LINK_LIBRARIES "Threads::Threads;atomic;${CMAKE_DL_LIBS};m;rt;X11::X11;X11::Xext;X11::Xrandr;X11::Xcomposite;X11::Xdamage;X11::Xfixes;X11::Xrender;X11::Xtst"
)

add_executable(webrtc_demo main.cc)
target_link_libraries(webrtc_demo PRIVATE WebRTC::WebRTC)
set_target_properties(webrtc_demo PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS OFF
)
```

`WebRTC::WebRTC` 封装了库路径、头文件和链接依赖。在自己的工程中保留这部分配置，把 `webrtc_demo` 换成自己的目标名即可。

可运行的示例已经放在 `~/webrtc-m94-cmake-demo`，其中 `main.cc` 展示了工厂、PeerConnection 和 DataChannel 的创建方法。

```bash
cd ~/webrtc-m94-cmake-demo
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
./build/webrtc_demo
```

### 当前项目的构建

项目位于 `~/mycode/cpp/WebRTC-Call`：

- `include/client/` 放头文件，`src/client/` 放实现，统一编译为 `client` 静态库。
- `pusher`、`puller` 分别使用 `src/pusher.cpp`、`src/puller.cpp`，链接 `client`。
- `client` 统一传递 WebRTC M94、libhv、SDL2 的头文件和链接依赖，使用 C++14。

```bash
cd ~/mycode/cpp/WebRTC-Call
cd build
cmake ..
ninja -j4
```

构建会生成 `client`、`pusher`、`puller`，可执行文件输出到 `build/bin/`。

WebRTC 默认使用 `~/webrtc-checkout/src`，可用 `-DWEBRTC_ROOT=...`、`-DWEBRTC_OUT=...` 指定其他路径。libhv 使用项目内的 `third_party/libhv/include` 和 `third_party/libhv/lib/libhv.so`，来自本机编译产物；SDL2 使用系统安装的 `libsdl2-dev`。

JSON 使用参考项目提供的 nlohmann/json 3.11.3，单头文件存放在 `third_party/json/include/json.hpp`。



运行可执行程序

```
./bin/pusher "http://localhost:8080/index/api/webrtc?app=live&stream=test&type=push"
./bin/puller "http://localhost:8080/index/api/webrtc?app=live&stream=test&type=play"
```

wsl没有摄像头设备，可能会启动失败

可以只验证拉流

已验证：测试端向 ZLMediaKit 推送 320×180 视频，C++ `puller` 窗口能够持续显示画面，并正常退出。实际拉流时，URL 对应的流需要已经推送到服务器。



## WebRTCClient：推拉流的对外入口

业务代码调用这个类发起、停止推拉流；它按 URL 管理多个 `WebRTCStream`，把操作投递到自己的线程执行。

**成员变量**

| 成员                  | 作用                                                         |
| --------------------- | ------------------------------------------------------------ |
| `thread_`             | 执行推拉流管理任务的 `rtc::Thread`                           |
| `signal_peer_client_` | 本客户端共用的信令对象，创建会话时传给 `WebRTCStream`        |
| `streams_`            | `URL → WebRTCStream` 的映射，用于查找会话、拒绝同一 URL 重复启动 |

**成员函数**

| 函数              | 作用                                                         |
| ----------------- | ------------------------------------------------------------ |
| `WebRTCClient()`  | 创建并启动管理线程，创建信令客户端                           |
| `~WebRTCClient()` | 关闭会话，停止 HTTP 客户端和事件循环，最后停止 WebRTC 管理线程 |
| `StartPush()`     | 根据 `audio`、`video` 参数创建推流会话，成功发起后保存到 `streams_` |
| `StartPull()`     | 接收 `url`、`audio`、`video`、`renderer`、`callback`，创建会话并传入渲染器 |
| `StopPush()`      | 同步关闭当前会话，清空 `streams_`，等待管理线程完成操作       |
| `StopPull()`      | 同步解除视频回调、关闭当前会话并清空 `streams_`              |

`callback(success, msg)` 在任务执行后报告本次操作的发起结果。后续连接状态通过 WebRTC 的观察者回调报告。





### rtc::Thread

这是 webrtc 提供的管理线程和任务调度的类，它封装了一个线程循环，并提供了几个重要的接口实现跨线程的任务同步与异步执行

* `rtc::Thread::Create`，通过这个方法可以获取到`std::unique_ptr<rtc::Thread>`
* `Invoke`，投递任务同步等待结果
* `PostTask`，投递任务，异步执行
* `PostDelayedTask`，将任务投递给线程异步执行，并指定多久之后执行



### scoped_refptr

webrtc 使用的引用计数类型的智能指针，相比 shared_ptr，他是一个侵入式引用计数的智能指针

* `RefCounter`，包含了`std::atomic<int>`的成员变量，提供了`AddRef`和`Release`，用于增减引用计数
* `RefCounterInterface`，接口类，使用 scoped_refptr 的类必须继承该类
* `RefCoutedObject`，包含了一个`RefCounter`成员变量



它的引用计数是对象自己维护，源码中调用了`AddRef` 和`Release`，所以我们一定要实现这两个方法，可以自己实现，也可以继承别人



## WebRTCStream：一条推流或拉流会话

负责创建 WebRTC 连接、添加音视频轨道、生成 Offer 和处理 Answer。它同时继承两个观察者接口：`PeerConnectionObserver` 接收连接和远端轨道事件，`CreateSessionDescriptionObserver` 接收 Offer 创建结果。

流是一个长连接 + 媒体语义的概念

**成员变量**

| 成员             | 作用                                                         |
| ---------------- | ------------------------------------------------------------ |
| `signal_client_` | 共享的信令客户端，负责把本地 Offer 发到服务器                |
| `peer_connection_factory_` | WebRTC 工厂，用于创建连接、音频源及音视频轨道，并配置编解码器工厂 |
| `peer_connection_` | 当前会话的 `PeerConnection`，负责 SDP 协商和媒体传输         |
| `url_`           | 当前推流或拉流的接口地址，也是发送 SDP 的目标地址            |
| `renderer_`      | 借用拉流主程序创建的渲染器；远端视频轨道到达后交给它显示，主程序负责销毁 |

**会话管理函数**

| 函数                            | 作用                                                         |
| ------------------------------- | ------------------------------------------------------------ |
| `WebRTCStream()`                | 保存信令客户端，将工厂和连接指针设为空                       |
| `~WebRTCStream()`               | 关闭连接，释放本类持有的连接和工厂引用                       |
| `StartPush()`                   | 保存 URL，初始化连接，添加本地轨道，发起不请求接收音视频的 Offer |
| `StartPull()`                   | 保存 URL 和渲染器，按 `audio`、`video` 参数发起接收音视频的 Offer |
| `StopPush()`                    | 调用 `CloseRTCPeerConnection()` 关闭连接                     |
| `StopPull()`                    | 移除视频帧回调、关闭连接，将借用的渲染器指针置空             |
| `InitializeRTCPeerConnection()` | 创建工厂和连接，采用 Unified Plan，启用 DTLS-SRTP，并注册自己为连接观察者 |
| `CloseRTCPeerConnection()`      | 调用 `peer_connection_->Close()`，释放本类持有的连接和工厂引用 |
| `AddTracks()`                   | 按参数创建音频轨道；视频通过摄像头采集源创建轨道，再用 `AddTrack()` 加入连接 |
| `CreateOffer()`                 | 设置是否接收音视频，再调用 WebRTC 异步创建 Offer             |
| `OnRemoteSdp()`                 | 将服务器返回的字符串解析为 SDP Answer，调用 `SetRemoteDescription()` |

**观察者回调**

| 回调                      | 触发时机与本例处理                                           |
| ------------------------- | ------------------------------------------------------------ |
| `OnSuccess()`             | 设置本地 Offer，通过 HTTP 发送；将返回的 Answer 投递回 WebRTC 线程处理 |
| `OnFailure()`             | Offer 创建失败：记录错误日志                                 |
| `OnAddTrack()`            | 远端轨道到达：如果是视频轨道，调用渲染器的 `StartRemoteRenderer()` |
| `OnIceConnectionChange()` | ICE 连接状态变化：记录状态日志                               |
| `OnSignalingChange()`     | 信令状态变化；本例为空实现                                   |
| `OnRemoveTrack()`         | 远端轨道移除；本例为空实现                                   |
| `OnDataChannel()`         | 远端创建数据通道；本例为空实现                               |
| `OnIceGatheringChange()`  | ICE 候选收集状态变化；本例为空实现                           |
| `OnIceCandidate()`        | 发现本地 ICE 候选；本例为空实现，没有在这里单独发送候选      |

注意区分两件事：`CreateOffer()` 是发起请求，`OnSuccess()` 才拿到生成的 Offer；`OnRemoteSdp()` 拿到的是服务器返回的 Answer。



### 推流流程

1.创建 RTCPeerConnection

2.添加发送轨道到 RTCPeerConnection

3.生成 offer sdp，与 ZLMediaKit 进行媒体协商

4.webrtc 底层自动完成 ICE，DTLS，之后开始推SRTP数据



#### 拉流流程

1.创建 RTCPeerConnection

2.生成 offer sdp，与 ZLMediaKit 进行媒体协商

3.webrtc 底层自动完成 ICE，DTLS，之后开始拉流



### 创建 RTCPeerConnection

首先创建 RTCPeerConnectionFactory，由它创建 Con

然后`CreatePeerConectionFactory`又一些参数

* network_thread：网络线程，负责网络收发包，传输层协议，比如 ICE，DTLS
* worker_thread：工作线程，负责一些CPU密集型工作，比如音视频解码
* signaling_thread：信令线程，RTCPeerConnection的对外 API 调用，observer的回调都在这个线程
* default_adm：音频设备层接口，管理音频系统设备，采集播放音频等，通过传NULL即可，除非diy
* encoder_factory/decoder_factory：自定义编解码工厂处理
* audio_mixer：多路接收音频混音器，如果需要自定义混音效果可以传入该参数，否则传NULL
* audio_processing：音频处理控制器，传NULL表示使用 webrtc 默认配置



### Track

在 WebRTC中，Track是媒体流的最小单位，代表一路独立的音视频数据

WebRTC的媒体层次结构

```
MediaStream（媒体流）
  ├── AudioTrack（音频轨）
  └── VideoTrack（视频轨）
```

音频和视频是独立传输的，在 WebRTC 中，音频和视频使用不同的编解码器，有各自的时间戳，采样率，帧率



一条 Track 有明确的方向

* sendonly
* recvonly
* sendrecv
* inactive



一条 Track 的生命周期

1. 创建 Source：音频源（麦克风）或视频源（摄像头）。
2. 创建 Track：`CreateAudioTrack` / `CreateVideoTrack`，把 Source 绑定到 Track 上。
3. 添加到 PeerConnection：`AddTrack`，Track 进入 SDP 协商。
4. 远端收到 Track：对方通过 `OnAddTrack` 回调感知到你的 Track。
5. 移除 Track：`RemoveTrack`，或关闭 PeerConnection 时自动结束。



## SignalPeerClient：HTTP 信令客户端

使用 libhv 的 HTTP 客户端发送 SDP，并把服务器返回的 SDP 交给调用方。

**成员变量**

| 成员              | 作用                                                |
| ----------------- | --------------------------------------------------- |
| `loop_`           | 本类创建的 libhv 事件循环，传给 `loop_thread_`       |
| `loop_thread_`    | 运行上述事件循环的线程对象                          |
| `http_client_`    | 发起异步 HTTP 请求的客户端，真正的 SDP 请求由它发送 |

**成员函数**

| 函数                  | 作用                                                         |
| --------------------- | ------------------------------------------------------------ |
| `SignalPeerClient()`  | 创建事件循环、事件线程和 HTTP 客户端，启动事件线程           |
| `~SignalPeerClient()` | 调用 `Stop()`，完成 HTTP 客户端和事件线程的退出             |
| `Stop()`              | 释放 HTTP 客户端，停止事件循环，并等待事件线程退出           |
| `SendSdp()`           | 将 Offer 放入 HTTP POST 请求体；检查响应状态和 JSON 中的 `code`，再通过回调返回 `sdp` |



可以看一下`ZLMediaKit`的 webrtc信令交互格式

[webrtc信令交互格式 · ZLMediaKit/ZLMediaKit Wiki](https://github.com/ZLMediakit/ZLMediakit/wiki/webrtc信令交互格式)



我们需要一个http客户端，不想自己实现，可以使用第三方库，比如`libhv`

### libhv

下载并且编译

```
$ git clone https://github.com/ithewei/libhv.git
$ cd libhv
$ mkdir build && build
$ cmake ..
$ make -j4
```

项目中引入 libhv

将 include 目录和 libhv.so 拷贝过去就行了，然后修改一下cmake就行了





## VcmCapture：摄像头采集和视频帧分发

封装 WebRTC 的摄像头采集模块。它既是接收摄像头帧的 **Sink**，又是向视频轨道提供帧的 **Source**。

**成员变量**

| 成员            | 作用                                                    |
| --------------- | ------------------------------------------------------- |
| `width_`        | 请求采集的视频宽度                                      |
| `height_`       | 请求采集的视频高度                                      |
| `fps_`          | 请求采集的帧率                                          |
| `device_id_`    | 要打开的摄像头设备标识                                  |
| `vcm_`          | 实际执行摄像头采集的 `VideoCaptureModule`               |
| `vcapture_cap_` | 传给采集模块的格式配置，包含宽高、帧率、I420 像素格式等 |
| `broadcaster_`  | 保存接收者列表，把收到的视频帧分发给订阅的 Sink         |

**成员函数**

| 函数                | 作用                                                         |
| ------------------- | ------------------------------------------------------------ |
| `VcmCapture()`      | 保存宽高、帧率和设备 ID；构造函数为私有，通过 `Create()` 创建 |
| `~VcmCapture()`     | 当前函数体为空，没有显式调用 `Destroy()`                     |
| `Create()`          | 创建采集对象并调用 `Init()`，成功返回指针，失败返回空指针    |
| `Init()`            | 打开摄像头，注册自己为采集回调，设置采集格式并启动采集       |
| `Destroy()`         | 停止采集，注销采集回调，释放本类持有的采集模块引用           |
| `AddOrUpdateSink()` | 将接收者和它的帧需求登记到 `broadcaster_`                    |
| `RemoveSink()`      | 从 `broadcaster_` 移除接收者                                 |
| `OnFrame()`         | 收到摄像头帧后，调用 `broadcaster_.OnFrame()` 向下游分发     |



```
摄像头硬件
    ↓ 生产帧
VideoCaptureModuleV4L2（vcm_）
    ↓ 回调 OnFrame
VcmCapture          
    ↓ broadcaster_ 分发
上层（CamVideoTrackSource → VideoTrack → PeerConnection）
    ↓ 消费帧
编码器 → 网络 → 远端
```



## CamVideoTrackSource：把采集源适配为 WebRTC 视频轨道源

继承 `webrtc::VideoTrackSource`，将 `VcmCapture` 提供的帧源包装成 WebRTC 能使用的视频轨道源，供 `CreateVideoTrack()` 创建视频轨道。

**成员变量**

| 成员           | 作用                                                   |
| -------------- | ------------------------------------------------------ |
| `vcm_capture_` | 保存外部传入的采集源裸指针；本类没有实现对它的删除逻辑 |

**成员函数**

| 函数                    | 作用                                             |
| ----------------------- | ------------------------------------------------ |
| `Create()`              | 创建引用计数管理的 `CamVideoTrackSource`         |
| `CamVideoTrackSource()` | 保存采集源，并通过基类构造参数将其标记为本地源   |
| `source()`              | 向 WebRTC 返回底层采集源，供视频轨道连接帧接收者 |



## VideoRenderer：接收远端视频帧并用 SDL2 显示

继承 `rtc::VideoSinkInterface<webrtc::VideoFrame>`，作为远端视频轨道的接收者。视频解码由 WebRTC 完成，这个类处理解码后的画面。

**成员变量**

| 成员        | 作用                                                  |
| ----------- | ----------------------------------------------------- |
| `track_`    | 保存远端视频轨道的引用，用于注册和移除视频帧回调      |
| `window_`   | SDL 窗口，作为画面的显示载体                          |
| `renderer_` | SDL 渲染器，负责将纹理绘制到窗口                      |
| `texture_`  | 保存视频像素的 SDL 纹理，使用与 I420 对应的 IYUV 格式 |
| `frame_mutex_` | 保护解码线程与显示线程之间的视频帧交接             |
| `pending_frame_` | 保存尚未显示的最新 I420 帧，新帧到来时覆盖旧帧   |
| `texture_width_`、`texture_height_` | 当前纹理的宽、高；视频分辨率变化时重建纹理 |

内嵌结构体 **`VideoWindowSetting`** 用来传递窗口参数：

| 成员 | 作用                     |
| ---- | ------------------------ |
| `x`  | 窗口左上角的水平位置     |
| `y`  | 窗口左上角的垂直位置     |
| `w`  | 窗口的宽度               |
| `h`  | 窗口的高度               |

**成员函数**

| 函数                    | 作用                                                         |
| ----------------------- | ------------------------------------------------------------ |
| `VideoRenderer()`       | 根据窗口参数创建 SDL 窗口和渲染器                           |
| `~VideoRenderer()`      | 移除视频帧回调，再依次销毁纹理、渲染器和窗口                 |
| `StartRemoteRenderer()` | 保存远端视频轨道，调用 `AddOrUpdateSink()` 注册自己          |
| `StopRemoteRenderer()`  | 调用 `RemoveSink()` 移除视频帧回调，释放轨道引用             |
| `OnFrame()`             | 在解码线程中把帧转换为 I420，保存到 `pending_frame_`          |
| `Render()`              | 在 SDL 主线程中取出最新帧，按实际尺寸创建或更新纹理，再绘制显示 |



## DummySetSessionDescriptionObserver：设置 SDP 的结果回调

定义在 `webrtc_stream.cpp` 内部，继承 `SetSessionDescriptionObserver`，负责接收 `SetLocalDescription()`、`SetRemoteDescription()` 的执行结果。

这个类没有自定义成员变量。

| 函数          | 作用                                              |
| ------------- | ------------------------------------------------- |
| `Create()`    | 创建引用计数管理的观察者对象，传给设置 SDP 的接口 |
| `OnSuccess()` | SDP 设置成功时记录日志                            |
| `OnFailure()` | SDP 设置失败时记录错误类型和原因                  |



