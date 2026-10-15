#include "client/webrtc_stream.h"

#include <api/audio_codecs/builtin_audio_encoder_factory.h> // WebRTC 内置的音频编解码器工厂（Opus、G.711 等）
#include <api/audio_codecs/builtin_audio_decoder_factory.h>
#include <api/video_codecs/builtin_video_encoder_factory.h> // WebRTC 内置的视频编解码器工厂（VP8、VP9、H.264 等）
#include <api/video_codecs/builtin_video_decoder_factory.h>
#include <api/create_peerconnection_factory.h> // 创建 PeerConnectionFactory 的入口函数
#include <api/task_queue/default_task_queue_factory.h> // 默认任务队列工厂，供信令线程和网络线程使用
#include <modules/video_capture/video_capture_factory.h> // 视频采集设备枚举
#include "client/cam_video_source.h" // 自定义的摄像头采集源和采集封装
#include "client/vcm_capture.h"
#include <rtc_base/logging.h> // WebRTC 日志宏
#include <rtc_base/thread.h>

// SetSessionDescriptionObserver 的简单实现
// SetLocalDescription / SetRemoteDescription 是异步操作，完成后通过这个观察者通知
// 这里只打日志，不做额外处理，所以叫 "Dummy"
class DummySetSessionDescriptionObserver : public webrtc::SetSessionDescriptionObserver
{
public:
    // 工厂方法，返回一个引用计数对象
    static DummySetSessionDescriptionObserver* Create()
    {
        return new rtc::RefCountedObject<DummySetSessionDescriptionObserver>();
    }
    virtual void OnSuccess()
    {
        RTC_LOG(LS_INFO) << "Set session description success";
    }
    virtual void OnFailure(webrtc::RTCError error)
    {
        RTC_LOG(LS_WARNING) << "Set session description failed, " << ToString(error.type()) << ": " << error.message();
    }
};

// 构造函数：只保存信令客户端，其他成员延迟到 StartPush/StartPull 时初始化
WebRTCStream::WebRTCStream(std::shared_ptr<SignalPeerClient> signal_client)
    : signal_client_(signal_client),
      peer_connection_factory_(nullptr),
      peer_connection_(nullptr)
{
}

// 析构时关闭会话
WebRTCStream::~WebRTCStream()
{
    CloseRTCPeerConnection();
}

// 信令状态变化，当前不处理
void WebRTCStream::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state)
{
}

// 远端新增一路 Track 时触发
// 如果是视频 Track，就交给 renderer_ 去渲染
void WebRTCStream::OnAddTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
    const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams)
{
    RTC_LOG(LS_INFO) << "WebRTCStream OnAddTrack, kind: " << receiver->track()->kind();

    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track = receiver->track();

    if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind && renderer_)
    {
        // 把基类 Track 强转为 VideoTrackInterface，交给渲染器
        rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track = static_cast<webrtc::VideoTrackInterface*>(track.get());
        renderer_->StartRemoteRenderer(video_track);
    }
}

// 远端移除 Track，当前不处理
void WebRTCStream::OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver)
{

}

// 远端创建 DataChannel，当前不处理
void WebRTCStream::OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel)
{

}

// ICE 连接状态变化，当前只打日志
// 实际项目中应该在这里处理断线重连、通知上层"连接成功/失败"等
void WebRTCStream::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state)
{
    RTC_LOG(LS_INFO) << "OnIceConnectionChange, new_state: " << new_state;
}

// ICE 收集状态变化，当前不处理
void WebRTCStream::OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state)
{

}

// 收集到本地 ICE Candidate，当前不处理
// 正常流程这里应该把 candidate 通过信令发给对方，否则连接可能建立失败
void WebRTCStream::OnIceCandidate(const webrtc::IceCandidateInterface* candidate)
{

}

// SDP 创建成功（Offer 或 Answer 生成完毕）
void WebRTCStream::OnSuccess(webrtc::SessionDescriptionInterface* desc)
{
    std::unique_ptr<webrtc::SessionDescriptionInterface> description(desc);
    if (!peer_connection_) {
        return;
    }
    std::string sdp;
    description->ToString(&sdp);
    peer_connection_->SetLocalDescription(DummySetSessionDescriptionObserver::Create(), description.release());

    // HTTP 回复回到信令线程后再操作 PeerConnection
    rtc::scoped_refptr<WebRTCStream> self(this);
    auto* signaling_thread = rtc::Thread::Current();
    signal_client_->SendSdp(url_, sdp, [self, signaling_thread](const std::string& answer){
        signaling_thread->PostTask(RTC_FROM_HERE, [self, answer](){
            self->OnRemoteSdp(answer);
        });
    });
}

// SDP 创建失败
void WebRTCStream::OnFailure(webrtc::RTCError error)
{
    RTC_LOG(LS_WARNING) << "Create session description failed, " << ToString(error.type()) << ": " << error.message();
}

// 启动推流：初始化连接、添加本地 Track、创建 Offer
// 返回的 bool 只代表"启动流程有没有出错"，不代表推流已经成功建立
std::pair<bool, std::string> WebRTCStream::StartPush(const std::string& url, bool audio, bool video)
{
    bool success = false;
    url_ = url;

    success = InitializeRTCPeerConnection();
    if (!success) {
        return std::make_pair(false, "InitializeRTCPeerConnection failed");
    }
    success = AddTracks(audio, video);
    if (!success) {
        return std::make_pair(false, "AddTracks failed");
    }
    // 推流方不主动接收对方的音视频，所以传 false, false
    success = CreateOffer(false, false);
    if (!success) {
        return std::make_pair(false, "CreateOffer failed");
    }

    return std::make_pair(true, "");
}

// 停止推流：关闭 PeerConnection
std::pair<bool, std::string> WebRTCStream::StopPush()
{
    CloseRTCPeerConnection();

    return std::make_pair(true, "");
}

// 启动拉流：初始化连接、创建 Offer（声明想接收音视频）
std::pair<bool, std::string> WebRTCStream::StartPull(const std::string& url, bool audio, bool video, VideoRenderer* renderer)
{
    url_ = url;
    renderer_ = renderer;
    if (!InitializeRTCPeerConnection()) {
        return std::make_pair(false, "InitializeRTCPeerConnection failed");
    }
    if (!CreateOffer(audio, video)) {
        return std::make_pair(false, "CreateOffer failed");
    }
    return std::make_pair(true, "");
}

// 停止拉流并解除对渲染器的引用
std::pair<bool, std::string> WebRTCStream::StopPull()
{
    if (renderer_) {
        renderer_->StopRemoteRenderer();
    }
    CloseRTCPeerConnection();
    renderer_ = nullptr;
    return std::make_pair(true, "");
}

// 初始化 PeerConnectionFactory 和 PeerConnection
bool WebRTCStream::InitializeRTCPeerConnection()
{
    // 创建 PeerConnectionFactory
    // 参数依次为：网络线程、工作线程、信令线程、音频设备、音频编码器、音频解码器、视频编码器、视频解码器、音频混合器、音频处理
    // 这里大部分传 nullptr，表示使用默认实现
    peer_connection_factory_ = webrtc::CreatePeerConnectionFactory(
        nullptr,   // 网络线程，nullptr 表示内部创建
        nullptr,   // 工作线程
        nullptr,   // 信令线程
        nullptr,   // 音频设备模块
        webrtc::CreateBuiltinAudioEncoderFactory(),
        webrtc::CreateBuiltinAudioDecoderFactory(),
        webrtc::CreateBuiltinVideoEncoderFactory(),
        webrtc::CreateBuiltinVideoDecoderFactory(),
        nullptr,   // 音频混合器
        nullptr    // 音频处理
    );
    if (!peer_connection_factory_) {
        return false;
    }

    webrtc::PeerConnectionInterface::RTCConfiguration config;
    // 使用 Unified Plan，一路 Track 一个 m-line，是当前推荐语义
    config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
    // 启用 DTLS-SRTP，媒体加密
    config.enable_dtls_srtp = true;

    // 创建 PeerConnection，this 作为观察者接收回调
    peer_connection_ = peer_connection_factory_->CreatePeerConnection(config, nullptr, nullptr, this);
    if (!peer_connection_) {
        return false;
    }

    return true;
}

// 关闭 PeerConnection 和 Factory
void WebRTCStream::CloseRTCPeerConnection()
{
    if (peer_connection_) {
        peer_connection_->Close();
        peer_connection_ = nullptr;
    }
    if (peer_connection_factory_) {
        peer_connection_factory_ = nullptr;
    }
}

// 添加本地音视频 Track，Track 是媒体流的最小单位
bool WebRTCStream::AddTracks(bool audio, bool video)
{
    if (audio)
    {
        // 音频处理选项
        cricket::AudioOptions options;
        // 创建音频 Track 和音频源
        auto audio_track = peer_connection_factory_->CreateAudioTrack("audio_label", peer_connection_factory_->CreateAudioSource(options).get());

        // 把 Track 添加到 PeerConnection，stream_id 用于标识所属的流
        auto result_or_error = peer_connection_->AddTrack(audio_track, { "stream_id" });
        if (!result_or_error.ok()) {
            RTC_LOG(LS_ERROR) << "Failed to add audio track to rtc peerconnection, error: " << result_or_error.error().message();
            return false;
        }
    }

    if (video)
    {
        // 枚举视频采集设备
        std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(
            webrtc::VideoCaptureFactory::CreateDeviceInfo());
        if (!info) {
            return false;
        }
        for (uint32_t i = 0; i < info->NumberOfDevices(); ++i) {
            char name[256] = { 0 };
            char id[256] = { 0 };
            if (info->GetDeviceName(i, name, sizeof(name), id, sizeof(id)) != 0) {
                RTC_LOG(LS_ERROR) << "Failed to get device name";
                continue;
            }
            // 创建采集源，分辨率 640x480，帧率 30
            rtc::VideoSourceInterface<webrtc::VideoFrame>* capture_source = VcmCapture::Create(640, 480, 30, id);
            
            // 创建视频 Track
            auto video_track = peer_connection_factory_->CreateVideoTrack("video_label", CamVideoTrackSource::Create(capture_source));

            auto result_or_error = peer_connection_->AddTrack(video_track, { "stream_id" });
            if (!result_or_error.ok()) {
                RTC_LOG(LS_ERROR) << "Failed to add video track to PeerConnection" << result_or_error.error().message();
                continue;
            }

            // 只使用第一个可用设备
            break;
        }
    }

    return true;
}

// 创建 Offer
bool WebRTCStream::CreateOffer(bool receive_audio, bool receive_video)
{
    if (!peer_connection_) {
        RTC_LOG(LS_WARNING) << "CreateOffer rtc peer connection not init";
        return false;
    }

    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
    // 是否在 Offer 里声明希望接收对方的音频/视频
    options.offer_to_receive_audio = receive_audio ? 1 : 0;
    options.offer_to_receive_video = receive_video ? 1 : 0;
    // 异步创建 Offer，结果通过 OnSuccess/OnFailure 回调
    peer_connection_->CreateOffer(this, options);

    return true;
}

// 收到远端 SDP（Answer）后的处理
void WebRTCStream::OnRemoteSdp(const std::string& sdp)
{
    if (!peer_connection_) {
        return;
    }

    // 把字符串解析成 SessionDescriptionInterface
    webrtc::SdpParseError err;
    std::unique_ptr<webrtc::SessionDescriptionInterface> session_desc =
        webrtc::CreateSessionDescription(webrtc::SdpType::kAnswer, sdp, &err);
    if (!session_desc) {
        return;
    }

    // 设置远端描述，session_desc.release() 因为 SetRemoteDescription 会接管所有权
    peer_connection_->SetRemoteDescription(DummySetSessionDescriptionObserver::Create(), session_desc.release());
}