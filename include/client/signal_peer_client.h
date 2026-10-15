#pragma once
#ifndef SIGNAL_PEER_CLIENT_H
#define SIGNAL_PEER_CLIENT_H

#include <iostream>
#include <vector>
#include <hv/EventLoop.h>
#include <hv/EventLoopThread.h>
#include <hv/WebSocketChannel.h>
#include <hv/HttpClient.h>
#include <functional>

// 这个类负责和信令服务器进行通信，交换 SDP 信息
class SignalPeerClient : public std::enable_shared_from_this<SignalPeerClient>{
public:
    SignalPeerClient();
    ~SignalPeerClient();

    void SendSdp(const std::string& url, const std::string& sdp, std::function<void(const std::string&)> sdp_callback);
    void Stop();

private:
    std::shared_ptr<hv::EventLoop> loop_;
    std::shared_ptr<hv::EventLoopThread> loop_thread_;
    std::shared_ptr<hv::HttpClient> http_client_;
};

#endif // SIGNAL_PEER_CLIENT_H