#include "client/webrtc_client.h"
#include "client/webrtc_stream.h"
#include <rtc_base/ref_counted_object.h>

WebRTCClient::WebRTCClient() : thread_(rtc::Thread::Create()){
    thread_->SetName("webrtc_client_thread", nullptr);
    thread_->Start();

    signal_peer_client_ = std::make_shared<SignalPeerClient>();
}

WebRTCClient::~WebRTCClient(){
    StopPush();
    signal_peer_client_->Stop();
    thread_->Stop();
}

void WebRTCClient::StartPush(const std::string& url, bool audio, bool video, CallBack callback){
    thread_->PostTask(RTC_FROM_HERE, [this, url, audio, video, callback](){
        bool success = false;
        std::string msg;

        if(streams_.count(url)){
            msg = "Stream already exists";
        }else{
            // 调用会话进行推送
            rtc::scoped_refptr<WebRTCStream> stream =
                new rtc::RefCountedObject<WebRTCStream>(signal_peer_client_);
            auto result = stream->StartPush(url, audio, video);
            success = result.first;
            msg = result.second;

            if(success){
                streams_[url] = stream;
            }
        }
        if(callback){
            callback(success, msg); // 通过回调函数返回结果
        }
    });
}

void WebRTCClient::StopPush(){
    thread_->Invoke<void>(RTC_FROM_HERE, [this](){
        for(auto& stream : streams_){
            stream.second->StopPush();
        }
        streams_.clear();
    });
}

void WebRTCClient::StartPull(const std::string& url, bool audio, bool video, VideoRenderer* renderer, CallBack callback){
    thread_->PostTask(RTC_FROM_HERE, [this, url, audio, video, renderer, callback](){
        bool success = false;
        std::string msg;

        if(streams_.count(url)){
            msg = "Stream already exists";
        }else{
            rtc::scoped_refptr<WebRTCStream> stream =
                new rtc::RefCountedObject<WebRTCStream>(signal_peer_client_);
            auto result = stream->StartPull(url, audio, video, renderer);
            success = result.first;
            msg = result.second;

            if(success){
                streams_[url] = stream;
            }
        }
        if(callback){
            callback(success, msg); // 通过回调函数返回结果
        }
    });
}

void WebRTCClient::StopPull(){
    thread_->Invoke<void>(RTC_FROM_HERE, [this](){
        for(auto& stream : streams_){
            stream.second->StopPull();
        }
        streams_.clear();
    });
}

