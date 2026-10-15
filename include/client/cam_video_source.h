#pragma once
#ifndef __CAM_VIDEO_SOURCE_H__
#define __CAM_VIDEO_SOURCE_H__

// VideoTrackSource 是 WebRTC 提供的基类
// 它本身也是一个 VideoSourceInterface，可以把视频帧分发给多个 Sink
// 同时它还能被包装成 VideoTrack，添加到 PeerConnection 上
#include <pc/video_track_source.h>

// CamVideoTrackSource 把"摄像头采集源"适配成"WebRTC 的 VideoTrackSource"
// 这样采集到的视频帧才能进入 WebRTC 的 Track 体系，被发送到远端
class CamVideoTrackSource : public webrtc::VideoTrackSource
{
public:
    // 工厂方法：创建并返回一个引用计数的 CamVideoTrackSource
    // 参数 source 是底层的摄像头采集源（VcmCapture 等）
    // 返回 scoped_refptr 是因为 WebRTC 内部对象需要引用计数管理生命周期
    static rtc::scoped_refptr<CamVideoTrackSource> Create(rtc::VideoSourceInterface<webrtc::VideoFrame>* source);

protected:
    // 构造函数，protected 表示不允许外部直接 new，必须通过 Create()
    // 参数 source 是要包装的底层采集源
    CamVideoTrackSource(rtc::VideoSourceInterface<webrtc::VideoFrame>* source);

protected:
    // 重写基类的 source() 方法
    // VideoTrackSource 内部需要拿到真正的帧来源，才能把帧分发出去
    // 这里返回保存的 vcm_capture_
    rtc::VideoSourceInterface<webrtc::VideoFrame>* source() override;

private:
    // 底层摄像头采集源的指针
    rtc::VideoSourceInterface<webrtc::VideoFrame>* vcm_capture_;
};

#endif // __CAM_VIDEO_SOURCE_H__