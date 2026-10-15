#include "client/vcm_capture.h"
#include <modules/video_capture/video_capture_factory.h> // VideoCaptureFactory：用来创建具体平台的采集模块
#include <modules/video_capture/video_capture_defines.h> // 采集参数定义（分辨率、帧率、像素格式等）
#include <modules/video_capture/video_capture.h>
#include <rtc_base/logging.h>
#include <rtc_base/task_utils/to_queued_task.h> 

// 工厂方法：创建 VcmCapture 并完成初始化
VcmCapture* VcmCapture::Create(int32_t width, int32_t height, int32_t fps, const std::string& device_id)
{
    VcmCapture* vc = new VcmCapture(width, height, fps, device_id);
    if (vc && vc->Init()) {
        return vc;
    }
    // 初始化失败时没有 delete，存在内存泄漏
    // 更严谨的做法是 delete vc 后再 return nullptr
    return nullptr;
}

// 构造函数：只保存参数，不做任何资源创建
// 真正的初始化放在 Init() 里，方便失败时统一处理
VcmCapture::VcmCapture(int32_t width, int32_t height, int32_t fps, const std::string& device_id)
    : width_(width),
      height_(height),
      fps_(fps),
      device_id_(device_id)
{

}

VcmCapture::~VcmCapture()
{
    Destroy();
}

// 初始化：创建 VCM 模块、注册回调、启动采集
bool VcmCapture::Init()
{
    // 根据设备 ID 创建采集模块
    // 在 Linux 上底层是 VideoCaptureModuleV4L2，通过 /dev/video* 访问摄像头
    vcm_ = webrtc::VideoCaptureFactory::Create(device_id_.c_str());
    if (!vcm_) {
        return false;
    }

    // 把自己注册为采集数据的回调
    // 之后采集到的每一帧都会回调到 this->OnFrame
    vcm_->RegisterCaptureDataCallback(this);

    // 配置采集参数
    vcapture_cap_.width = width_;
    vcapture_cap_.height = height_;
    vcapture_cap_.maxFPS = fps_;
    // 请求 I420 格式，这是 WebRTC 内部最常用的视频帧格式
    vcapture_cap_.videoType = webrtc::VideoType::kI420;
    // 非隔行扫描
    vcapture_cap_.interlaced = false;

    // 启动采集，返回 0 表示成功
    if (vcm_->StartCapture(vcapture_cap_) != 0)
    {
        RTC_LOG(LS_ERROR) << "Failed to start capture";
        // 启动失败，清理已创建的资源
        Destroy();
        return false;
    }

    return true;
}

// 销毁：停止采集、注销回调、释放 VCM
void VcmCapture::Destroy()
{
    if (vcm_)
    {
        // 先停止采集，确保不再产生新帧
        vcm_->StopCapture();
        // 再注销回调，防止停止过程中还有帧回调进来
        vcm_->DeRegisterCaptureDataCallback();
        // 释放引用，vcm_ 由 scoped_refptr 管理，减到 0 时自动销毁
        vcm_ = nullptr;
    }
}

// VideoSourceInterface 接口：添加上层 Sink
// 上层（如 CamVideoTrackSource）通过这个方法注册，之后就能收到帧
void VcmCapture::AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink, const rtc::VideoSinkWants& wants)
{
    // 直接转发给 broadcaster_，由它维护 Sink 列表
    broadcaster_.AddOrUpdateSink(sink, wants);
}

// VideoSourceInterface 接口：移除上层 Sink
void VcmCapture::RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink)
{
    broadcaster_.RemoveSink(sink);
}

// VideoSinkInterface 接口：收到 VCM 采集的一帧
// 这是整个采集链路的入口，帧从这里进入你的代码
void VcmCapture::OnFrame(const webrtc::VideoFrame& frame) {
    // 把帧分发给所有注册的 Sink（上层消费者）
    // broadcaster_ 内部会处理每个 Sink 的 VideoSinkWants
    broadcaster_.OnFrame(frame);
}