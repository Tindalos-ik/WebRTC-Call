#pragma once
#ifndef _VIDEO_RENDERER_H_
#define _VIDEO_RENDERER_H_

#include <api/media_stream_interface.h> // WebRTC 媒体流接口，包含 VideoTrackInterface、VideoSinkInterface 等
#include <SDL2/SDL.h> // SDL2 库，用于创建窗口、渲染视频帧
#include <api/scoped_refptr.h>
#include <api/video/video_frame_buffer.h>
#include <mutex>

// VideoRenderer 负责把远端传来的视频帧显示到窗口上
// 它继承自 rtc::VideoSinkInterface<webrtc::VideoFrame>，
// 这是一个"视频接收器"接口，只要实现了 OnFrame，就能接收视频帧
class VideoRenderer : public rtc::VideoSinkInterface<webrtc::VideoFrame>
{
public:
    // 窗口位置和大小的配置结构
    // 用来指定视频窗口显示在屏幕的哪个位置、多大尺寸
    struct VideoWindowSetting {
        int x;  // 窗口左上角 x 坐标
        int y;  // 窗口左上角 y 坐标
        int w;  // 窗口宽度
        int h;  // 窗口高度
    };

public:
    // 构造时传入窗口配置，内部会创建 SDL 窗口和渲染器
    VideoRenderer(const VideoWindowSetting& setting);
    ~VideoRenderer();

    // 收到一帧视频时被 WebRTC 调用
    void OnFrame(const webrtc::VideoFrame& frame) override;

    // 在 SDL 主线程中显示最近收到的一帧
    void Render();

    // 开始渲染指定的远端视频 Track
    // 内部会把这个 renderer 自己注册为 track 的 sink，
    // 之后 track 有帧就会回调 OnFrame
    void StartRemoteRenderer(webrtc::VideoTrackInterface* track);
    void StopRemoteRenderer();

private:
    // 持有正在显示的远端视频轨道
    rtc::scoped_refptr<webrtc::VideoTrackInterface> track_;
    std::mutex frame_mutex_;
    rtc::scoped_refptr<webrtc::I420BufferInterface> pending_frame_;

    // SDL 窗口句柄
    SDL_Window* window_ = nullptr;
    // SDL 渲染器，负责把纹理画到窗口上
    SDL_Renderer* renderer_ = nullptr;
    // SDL 纹理，视频帧会先转成纹理，再渲染
    SDL_Texture* texture_ = nullptr;
    int texture_width_ = 0;
    int texture_height_ = 0;
};

#endif // !_VIDEO_RENDERER_H_