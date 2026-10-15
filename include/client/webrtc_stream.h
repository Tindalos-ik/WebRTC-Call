#pragma once
#ifndef WEBRTC_STREAM_H
#define WEBRTC_STREAM_H

#include <api/peer_connection_interface.h> // WebRTC 核心接口：PeerConnection、Track、DataChannel 等
#include "signal_peer_client.h"
#include <iostream>
#include "video_renderer.h" // 视频渲染器，负责把远端视频帧画到窗口上

class VideoRenderer;

// WebRTCStream 是整个通话的核心类，代表"一条流"的生命周期
// 它同时继承两个观察者接口：
//   1. PeerConnectionObserver：接收 PeerConnection 的状态变化、ICE、Track 等事件
//   2. CreateSessionDescriptionObserver：接收 SDP 创建（Offer/Answer）的成功或失败结果
class WebRTCStream : public webrtc::PeerConnectionObserver,
                     public webrtc::CreateSessionDescriptionObserver
{
public:
    // 构造时传入信令客户端，用于和信令服务器通信
    WebRTCStream(std::shared_ptr<SignalPeerClient> signal_client);
    virtual ~WebRTCStream();

    // ===== PeerConnectionObserver 回调 =====
    // 信令状态变化（Stable / HaveLocalOffer / HaveRemoteOffer 等）
    virtual void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override;
    // 远端新增了一路 Track（收到对方的音视频流时触发）
    virtual void OnAddTrack(
        rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
        const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams) override;
    // 远端移除了一路 Track
    virtual void OnRemoveTrack(
        rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) override;
    // 远端创建了 DataChannel（用于传非媒体数据，如文本、控制指令）
    virtual void OnDataChannel(
        rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override;
    // ICE 连接状态变化（Checking / Connected / Disconnected / Failed 等）
    virtual void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state) override;
    // ICE 收集状态变化（New / Gathering / Complete）
    virtual void OnIceGatheringChange(
        webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
    // 收集到一个本地 ICE Candidate，需要发给对方
    virtual void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;

    // ===== CreateSessionDescriptionObserver 回调 =====
    // SDP 创建成功（Offer 或 Answer 生成完毕）
    virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc) override;
    // SDP 创建失败
    virtual void OnFailure(webrtc::RTCError error) override;

public:
    // ===== 对外提供的接口 =====
    // 启动推流：url 是推流地址，audio/video 决定是否推送对应媒体
    // 返回 pair<是否成功, 错误信息或空字符串>
    // 注意：这是同步返回，实际推流结果是异步的，需要靠回调或状态变化确认
    std::pair<bool, std::string> StartPush(const std::string& url, bool audio = false, bool video = false);
    std::pair<bool, std::string> StartPull(const std::string& url, bool audio, bool video, VideoRenderer* renderer);
    std::pair<bool, std::string> StopPush();
    std::pair<bool, std::string> StopPull();

private:
    // 初始化 PeerConnectionFactory 和 PeerConnection
    bool InitializeRTCPeerConnection();
    // 关闭并释放 PeerConnection
    void CloseRTCPeerConnection();
    // 创建 Offer，receive_audio/video 表示期望接收对方的哪些媒体
    bool CreateOffer(bool receive_audio, bool receive_video);
    // 添加本地音视频 Track（摄像头、麦克风）
    bool AddTracks(bool audio, bool video);
    // 收到远端 SDP 后的处理逻辑（SetRemoteDescription 等）
    void OnRemoteSdp(const std::string& sdp);

private:
    // 信令客户端，负责和信令服务器通信
    std::shared_ptr<SignalPeerClient> signal_client_;
    // PeerConnection 工厂，创建 PeerConnection 的入口
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory_;
    // 实际的 PeerConnection 对象，代表与远端的一条连接
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
    // 当前流的 URL（推流或拉流地址）
    std::string url_;
    // 借用拉流端主线程管理的渲染器
    VideoRenderer* renderer_ = nullptr;
};

#endif // WEBRTC_STREAM_H