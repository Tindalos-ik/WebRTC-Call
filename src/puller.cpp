#include <iostream>
#include <csignal>
#include <atomic>
#include <exception>
#include <memory>
#include <SDL2/SDL.h>
#include <rtc_base/ssl_adapter.h>
#include "client/webrtc_client.h"
#include "client/video_renderer.h"

// 全局退出标志，信号处理函数和主循环都通过它通信
volatile std::sig_atomic_t is_exit = 0;
std::atomic<bool> start_failed{false};

// 信号处理函数：收到 Ctrl+C（SIGINT）时设置退出标志
void signal_handler(int) {
    is_exit = true;
}

// 拉流启动结果的回调
// success 为 true 表示启动流程没有出错，不代表已经收到帧
// 真正的连接状态要靠 OnIceConnectionChange 等回调确认
void pull_stream_callback(bool success, const std::string& msg) {
    if (success) {
        std::cout << "Pull Stream started successfully" << std::endl;
    } else {
        std::cout << "Failed to start pull stream: " << msg << std::endl;
        start_failed = true;
    }
}

int main(int argc, char *argv[]) {
    // 检查命令行参数：必须提供拉流地址
    if (argc != 2) {
        std::cout << "Usage: ./puller url" << std::endl;
        return 1;
    }
    std::string url = argv[1];

    // 注册 SIGINT（Ctrl+C）的处理函数
    signal(SIGINT, signal_handler);

    // 初始化 SDL 视频子系统
    // VideoRenderer 内部要用 SDL 创建窗口和渲染器，必须先初始化
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cout << "SDL Initialization failed: " << SDL_GetError() << std::endl;
        return 2;
    }
    if (!rtc::InitializeSSL()) {
        std::cerr << "WebRTC SSL initialization failed" << std::endl;
        SDL_Quit();
        return 2;
    }

    // 创建 WebRTC 客户端，负责和信令服务器、ZLMediaKit 通信
    auto webrtc_client = std::make_shared<WebRTCClient>();

    // 创建视频渲染器，窗口位置 (0,0)，大小 640x480
    std::unique_ptr<VideoRenderer> renderer;
    try {
        renderer.reset(new VideoRenderer(VideoRenderer::VideoWindowSetting{0, 0, 640, 480}));
    } catch (const std::exception& error) {
        std::cerr << "Video renderer initialization failed: " << error.what() << std::endl;
        webrtc_client.reset();
        rtc::CleanupSSL();
        SDL_Quit();
        return 2;
    }

    // 启动拉流：传入 URL、渲染器、结果回调
    // 这个调用会触发 SDP 协商、ICE 收集等异步流程
    webrtc_client->StartPull(url, true, true, renderer.get(), pull_stream_callback);

    // 主循环：处理 SDL 事件（窗口关闭、键盘输入等）
    // 收到退出标志或窗口关闭事件时结束
    SDL_Event event;
    while (!is_exit && !start_failed.load()) {
        // SDL_PollEvent 非阻塞地取一个事件
        // 如果没有事件，返回 0，循环继续
        if (SDL_PollEvent(&event)) {
            if (SDL_QUIT == event.type) {
                // 用户点了窗口的关闭按钮
                is_exit = true;
            }
        }
        renderer->Render();
        SDL_Delay(10);
    }

    // 停止拉流，释放 WebRTC 相关资源
    webrtc_client->StopPull();
    renderer.reset();
    webrtc_client.reset();
    rtc::CleanupSSL();

    // 退出 SDL，释放 SDL 相关资源
    SDL_Quit();

    std::cout << "puller exit" << std::endl;

    return start_failed.load() ? 1 : 0;
}