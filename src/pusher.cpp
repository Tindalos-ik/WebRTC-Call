#include <iostream>
#include <csignal>
#include <chrono>
#include <thread>
#include "client/webrtc_client.h"

// 全局退出标志，信号处理函数和主循环都通过它通信
bool is_exit = false;

// 信号处理函数：收到 Ctrl+C（SIGINT）时设置退出标志
void signal_handler(int) {
    is_exit = true;
}

// 推流启动结果的回调
// success 为 true 表示启动流程没有出错，不代表推流已经成功建立
// 真正的连接状态要靠 OnIceConnectionChange 等回调确认
void push_stream_callback(bool success, const std::string& msg) {
    if (success) {
        std::cout << "Push Stream started successfully" << std::endl;
    } else {
        std::cout << "Failed to start push stream: " << msg << std::endl;
        // 启动失败，直接设置退出标志，让主循环结束
        is_exit = true;
    }
}

int main(int argc, char *argv[]) {
    // 检查命令行参数：必须提供推流地址
    if (argc != 2) {
        std::cout << "Usage: ./pusher url" << std::endl;
        return 1;
    }
    std::string url = argv[1];

    // 注册 SIGINT（Ctrl+C）的处理函数
    signal(SIGINT, signal_handler);

    // 创建 WebRTC 客户端，负责和信令服务器、ZLMediaKit 通信
    auto webrtc_client = std::make_shared<WebRTCClient>();

    // 启动推流：传入 URL、是否推送音频、是否推送视频、结果回调
    // 这里 audio=true, video=true，表示音视频都推
    // 这个调用会触发 SDP 协商、ICE 收集等异步流程
    webrtc_client->StartPush(url, true, true, push_stream_callback);

    // 主循环：等待退出标志
    // 推流端不需要处理 UI 事件，所以只是短暂休眠，避免空转占满 CPU
    while (!is_exit) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    webrtc_client->StopPush();

    std::cout << "pusher exit" << std::endl;

    return 0;
}