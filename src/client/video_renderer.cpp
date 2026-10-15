#include "client/video_renderer.h"
#include <SDL2/SDL.h>
#include <rtc_base/logging.h>
#include <stdexcept>
#include <utility>

// 创建 SDL 窗口和渲染器，纹理按收到的视频帧尺寸创建
VideoRenderer::VideoRenderer(const VideoWindowSetting& setting)
{
    // SDL_WINDOW_SHOWN 表示窗口创建后立即显示
    window_ = SDL_CreateWindow(
        "Window",
        setting.x, setting.y, setting.w, setting.h,
        SDL_WINDOW_SHOWN
    );
    if (!window_) {
        throw std::runtime_error(SDL_GetError());
    }

    // 创建渲染器
    // 参数：关联窗口、驱动索引（-1 表示自动选择）、标志
    // SDL_RENDERER_ACCELERATED：使用硬件加速
    // SDL_RENDERER_PRESENTVSYNC：启用垂直同步，避免画面撕裂
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) {
        const std::string error = SDL_GetError();
        SDL_DestroyWindow(window_);
        throw std::runtime_error(error);
    }

}

// 析构函数：按创建的反序销毁 SDL 资源
VideoRenderer::~VideoRenderer()
{
    StopRemoteRenderer();
    if (texture_) {
        SDL_DestroyTexture(texture_);
    }
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
    }
    if (window_) {
        SDL_DestroyWindow(window_);
    }

}

// 解码线程只保存最新一帧，交给 SDL 主线程显示
void VideoRenderer::OnFrame(const webrtc::VideoFrame& frame)
{
    auto buffer = frame.video_frame_buffer()->ToI420();
    std::lock_guard<std::mutex> lock(frame_mutex_);
    pending_frame_ = std::move(buffer);
}

void VideoRenderer::Render()
{
    rtc::scoped_refptr<webrtc::I420BufferInterface> frame;
    {
        std::lock_guard<std::mutex> lock(frame_mutex_);
        frame.swap(pending_frame_);
    }
    if (!frame) {
        return;
    }

    const int width = frame->width();
    const int height = frame->height();
    const bool resized = width != texture_width_ || height != texture_height_;
    if (resized) {
        auto* texture = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_IYUV,
                                         SDL_TEXTUREACCESS_STREAMING, width, height);
        if (!texture) {
            RTC_LOG(LS_ERROR) << "SDL texture creation failed: " << SDL_GetError();
            return;
        }
        if (texture_) {
            SDL_DestroyTexture(texture_);
        }
        texture_ = texture;
        texture_width_ = width;
        texture_height_ = height;
    }

    if (SDL_UpdateYUVTexture(texture_, nullptr,
                            frame->DataY(), frame->StrideY(),
                            frame->DataU(), frame->StrideU(),
                            frame->DataV(), frame->StrideV()) != 0 ||
        SDL_RenderClear(renderer_) != 0 ||
        SDL_RenderCopy(renderer_, texture_, nullptr, nullptr) != 0) {
        RTC_LOG(LS_ERROR) << "SDL video display failed: " << SDL_GetError();
        return;
    }
    SDL_RenderPresent(renderer_);
    if (resized) {
        RTC_LOG(LS_INFO) << "Remote video displayed: " << width << "x" << height;
    }
}

// 开始渲染指定的远端视频 Track
void VideoRenderer::StartRemoteRenderer(webrtc::VideoTrackInterface* track)
{
    StopRemoteRenderer();
    // 把 this 注册为 track 的 sink，sink 与 sender 相对，sender发送数据，sink 接收数据
    // 之后 track 每收到一帧，就会调用 this->OnFrame
    // AddOrUpdateSink 如果已经注册过，会更新 sink 的 wants 配置
    // rtc::VideoSinkWants() 是默认的接收参数（不限制分辨率、帧率）
    track_ = track;
    track_->AddOrUpdateSink(this, rtc::VideoSinkWants());
}

void VideoRenderer::StopRemoteRenderer()
{
    if (track_) {
        track_->RemoveSink(this);
        track_ = nullptr;
    }
}