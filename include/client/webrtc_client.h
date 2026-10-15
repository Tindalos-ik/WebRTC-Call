#pragma once
#ifndef WEBRTC_CLIENT_H
#define WEBRTC_CLIENT_H

#include <map>
#include <api/scoped_refptr.h>
#include <rtc_base/thread.h>
#include "signal_peer_client.h"
#include "video_renderer.h"
#include "webrtc_stream.h"
#include <functional>
#include <memory>

class SignalPeerClient;
class WebRTCStream;

using CallBack = std::function<void(bool, const std::string&)>;

class WebRTCClient{
public:
    WebRTCClient();
    ~WebRTCClient();

    // 尝试推流
    // url: 推流地址
    // audio: 是否是音频流，即是否推送音频数据，默认只推视频
    // callback: 推流结果回调，true表示成功，false表示失败，失败原因在 string 中
    void StartPush(const std::string& url, bool audio = false,  bool video = false, CallBack callback= nullptr);
    // 停止推流
    void StopPush();
    // 尝试拉流
    void StartPull(const std::string& url, bool audio, bool video, VideoRenderer* renderer, CallBack callback = nullptr);
    // 停止拉流
    void StopPull();

private:
    std::unique_ptr<rtc::Thread> thread_; // webrtc 线程，提供了一些线程相关的操作
    std::shared_ptr<SignalPeerClient> signal_peer_client_; // 客户端共用的信令对象，创建会话时会传给 webrtcstream
    std::map<std::string, rtc::scoped_refptr<WebRTCStream>> streams_; // 流，用于推流和拉流，url->stream
};


#endif // WEBRTC_CLIENT_H