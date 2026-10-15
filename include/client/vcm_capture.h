#pragma once

#include <iostream>
#include <rtc_base/thread.h> // WebRTC 线程抽象，用于在指定线程上执行任务
#include <api/scoped_refptr.h>
#include <api/media_stream_interface.h> // 媒体流接口：VideoFrame、VideoSinkInterface 等
#include <api/video/video_frame.h> // 视频帧结构
#include <modules/video_capture/video_capture.h> // VideoCaptureModule：WebRTC 的视频采集模块
#include <modules/video_capture/video_capture_defines.h> // 采集参数定义（分辨率、帧率、格式等）
#include <media/base/video_broadcaster.h> // VideoBroadcaster：把一路帧分发给多个 Sink 的工具类
#include <pc/video_track_source.h> // VideoTrackSource 基类

// VcmCapture 是整个采集链路的核心，它同时扮演两个角色：
//   1. VideoSourceInterface：作为"数据源"，被上层（CamVideoTrackSource）消费
//   2. VideoSinkInterface：作为"接收器"，接收底层 VCM 采集模块的帧
//
// 数据流向：
//   VCM 采集模块 → OnFrame（VcmCapture 作为 Sink）→ broadcaster_ → 上层 Sink
class VcmCapture : public rtc::VideoSourceInterface<webrtc::VideoFrame>,
                   public rtc::VideoSinkInterface<webrtc::VideoFrame>
{
public:
    ~VcmCapture();

public:
    // 工厂方法：创建 VcmCapture 实例
    // 参数：宽、高、帧率、设备 ID
    // 返回裸指针，说明调用方需要自己管理生命周期（配合 Destroy() 使用）
    static VcmCapture* Create(int32_t width, int32_t height, int32_t fps, const std::string& device_id);

private:
    // 构造函数私有，强制通过 Create() 创建
    VcmCapture(int32_t width, int32_t height, int32_t fps, const std::string& device_id);

public:
    // 初始化：启动 VCM 采集模块，把自己注册为采集回调
    bool Init();
    // 销毁：停止采集，释放 VCM 资源
    void Destroy();

    // ===== VideoSourceInterface 接口 =====
    // 上层（如 CamVideoTrackSource）通过这两个方法注册/注销自己
    // 注册后，采集到的帧会通过 broadcaster_ 分发给他们

    // 添加或更新一个 Sink
    void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink, const rtc::VideoSinkWants& wants) override;

    // 移除一个 Sink
    virtual void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override;

    // ===== VideoSinkInterface 接口 =====
    // 底层 VCM 采集模块每采到一帧，就调用这个方法
    void OnFrame(const webrtc::VideoFrame& frame) override;

private:
    // 采集参数
    int32_t width_;       // 采集宽度
    int32_t height_;      // 采集高度
    int32_t fps_;         // 采集帧率
    std::string device_id_;  // 摄像头设备 ID

    // WebRTC 的视频采集模块，真正和摄像头驱动打交道的对象
    // scoped_refptr 表示它由引用计数管理生命周期
    rtc::scoped_refptr<webrtc::VideoCaptureModule> vcm_;

    // 采集能力描述：告诉 VCM 要什么分辨率、帧率、像素格式
    webrtc::VideoCaptureCapability vcapture_cap_;

    // 广播器：把 OnFrame 收到的帧分发给所有注册的 Sink
    // 它内部维护了一个 Sink 列表，支持一对多分发
    rtc::VideoBroadcaster broadcaster_;
};