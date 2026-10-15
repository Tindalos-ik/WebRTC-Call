#include "client/signal_peer_client.h"
#include <hv/HttpMessage.h>
#include <rtc_base/logging.h>
#include <json.hpp>

SignalPeerClient::SignalPeerClient():
    loop_(std::make_shared<hv::EventLoop>()),
    loop_thread_(std::make_shared<hv::EventLoopThread>(loop_)),
    http_client_(std::make_shared<hv::HttpClient>()){
    loop_thread_->start();
}

SignalPeerClient::~SignalPeerClient() {
    Stop();
}

void SignalPeerClient::Stop() {
    http_client_.reset();
    loop_->stop();
    loop_thread_->join();
}

void SignalPeerClient::SendSdp(const std::string& url, const std::string& sdp, std::function<void(const std::string&)> sdp_callback) {
    if(http_client_){
        auto req = std::make_shared<HttpRequest>();
        req->method = HTTP_POST;
        req->url = url;
        req->body = sdp;
        req->timeout = 10;
        http_client_->sendAsync(req, [sdp_callback](const std::shared_ptr<HttpResponse>& rsp){
            if(rsp == nullptr){
                RTC_LOG(LS_ERROR) << "SDP HTTP request failed";
                return;
            }
            if(rsp->status_code != 200){
                RTC_LOG(LS_ERROR) << "SDP HTTP status: " << rsp->status_code;
                return;
            }
            try {
                auto data = nlohmann::json::parse(rsp->body);
                if(data.value("code", -1) != 0){
                    RTC_LOG(LS_ERROR) << "ZLMediaKit rejected SDP: " << data.value("msg", std::string("unknown error"));
                    return;
                }
                sdp_callback(data.at("sdp").get<std::string>());
            } catch(const nlohmann::json::exception& error) {
                RTC_LOG(LS_ERROR) << "Invalid SDP response: " << error.what();
            }
        });
    }
}
