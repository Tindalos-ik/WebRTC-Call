#include "client/cam_video_source.h"

rtc::scoped_refptr<CamVideoTrackSource> CamVideoTrackSource::Create(
    rtc::VideoSourceInterface<webrtc::VideoFrame>* source)
{
    return new rtc::RefCountedObject<CamVideoTrackSource>(source);
}

CamVideoTrackSource::CamVideoTrackSource(rtc::VideoSourceInterface<webrtc::VideoFrame>* source)
    : webrtc::VideoTrackSource(false),
        vcm_capture_(source)
{
}

rtc::VideoSourceInterface<webrtc::VideoFrame>* CamVideoTrackSource::source()
{
    return vcm_capture_;
}
