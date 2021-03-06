#include <iostream>

#include "simplertc.h"
#include "peer.h"
#include "rtcstream.h"
#include "simplerenderer.h"
#include "simplecapturer.h"
#include "simpleaudiodevice.h"


enum {
    MSG_RTC_CALL,
    MSG_AUDIO_TIMER,
};

SimpleRTC::SimpleRTC(const std::string& myName, bool isCaller) {
    stream_ = NULL;
    peer_ = NULL;
    myName_ = myName;
    isCaller_ = isCaller;

    signal_thread_ = new talk_base::Thread();
    signal_thread_->Start();

#if 1
    adm_ = new webrtc::SimpleAudioDevice();
    signal_thread_->PostDelayed(10, this, MSG_AUDIO_TIMER);
    factory_ = webrtc::CreatePeerConnectionFactory(NULL, NULL, adm_, NULL, NULL); 
#else
    factory_ = webrtc::CreatePeerConnectionFactory(); 
#endif
    renderer_ = new SimpleVideoRenderer();

    capturer_ = new SimpleCapturer();    
    capturer_->set_enable_camera_list(true);
}

SimpleRTC::~SimpleRTC() {
    if ( signal_thread_ ) {
        signal_thread_->Stop();
        delete signal_thread_;
    }        
}

void SimpleRTC::Login(const std::string &server, 
               const unsigned short port) {
    peer_ =  new Peer(server, port, myName_, "simple", signal_thread_);
    peer_->SignalOnline.connect(this, &SimpleRTC::onOnLine);
    peer_->SignalOffline.connect(this, &SimpleRTC::onOffline);
    peer_->SignalRemoteOnline.connect(this, &SimpleRTC::onRemoteOnline);
    peer_->SignalRemoteOffline.connect(this, &SimpleRTC::onRemoteOffline);
    peer_->SignalRemoteMessage.connect(this, &SimpleRTC::onRemoteMessage);
    peer_->SignalPrintString.connect(this, &SimpleRTC::onPrintString);    //for log and debug
    peer_->Start();
}

void SimpleRTC::Run() {
    talk_base::Thread* main_thread = talk_base::Thread::Current();
    main_thread->Run();
}

void SimpleRTC::OnMessage(talk_base::Message *msg) { 
    switch (msg->message_id) {
        case MSG_RTC_CALL:
            makeCall();
            break;    

        case MSG_AUDIO_TIMER:
            signal_thread_->PostDelayed(10, this, MSG_AUDIO_TIMER);
            adm_->Test();
            break;
    }
}

void SimpleRTC::onOnLine(bool isOk) {

}

void SimpleRTC::onOffline() {
    std::cout << "[NATIVE]    Can't login to server, exiting..." << std::endl;
    exit(-1);
}

void SimpleRTC::onRemoteOnline(const std::string &remote, const std::string &role) {
    if ( stream_ == NULL && isCaller_ == true ) {
        stream_ = new RtcStream(remote, factory_, capturer_, renderer_);
        stream_->SignalSessionDescription.connect(this, &SimpleRTC::OnLocalDescription);
        stream_->SignalIceCandidate.connect(this, &SimpleRTC::OnLocalCandidate);        

        signal_thread_->PostDelayed(1000, this, MSG_RTC_CALL);
    }
}

void SimpleRTC::onRemoteOffline(const std::string &remote) {

}

void SimpleRTC::onRemoteMessage(const std::string &remote, const std::vector<std::string>& msgBody) {
    if ( msgBody.size() == 2 && msgBody[0] == "call" && msgBody[1] == "media" ) {
        if ( stream_ == NULL && isCaller_ == false) {
            
            stream_ = new RtcStream(remote, factory_, capturer_, renderer_);
            stream_->SignalSessionDescription.connect(this, &SimpleRTC::OnLocalDescription);
            stream_->SignalIceCandidate.connect(this, &SimpleRTC::OnLocalCandidate);        

            answerCall(); 
        }
    } else if ( msgBody.size() == 2 && msgBody[0] == "call" && msgBody[1] == "ok" ) {
        stream_->SetupLocalStream(true, true, false);
        stream_->CreateOfferDescription();    
    } else if ( msgBody.size() == 2 && msgBody[0] == "call" && msgBody[1] == "dataok" ) {
        stream_->SetupLocalStream(false, false, true);
        stream_->CreateOfferDescription();    
    } else if ( msgBody.size() == 3 && msgBody[0] == "rtc" && msgBody[1] == "desc" ) {
        if ( stream_ != NULL ) {
            stream_->SetRemoteDescription( msgBody[2] );
            if ( isCaller_ == false) {
                stream_->SetupLocalStream(true, true, false);
                stream_->CreateAnswerDescription(); 
            }
        } 
    } else if ( msgBody.size() == 3 && msgBody[0] == "rtc" && msgBody[1] == "cand" ) {
        stream_->SetRemoteCandidate(msgBody[2]);
    }        
}

void SimpleRTC::onPrintString(const std::string& msg) {
    //std::cout << msg << std::endl;
}

void SimpleRTC::OnLocalCandidate(RtcStream* stream, const std::string& cand) {
    std::vector<std::string> msgBody;
    msgBody.push_back( "rtc");
    msgBody.push_back( "cand");
    msgBody.push_back( cand );
    peer_->SendMessage( stream->id(), msgBody);
}

void SimpleRTC::OnLocalDescription(RtcStream* stream, const std::string& desc) {
    std::vector<std::string> msgBody;
    msgBody.push_back( "rtc");
    msgBody.push_back( "desc");
    msgBody.push_back( desc );
    peer_->SendMessage( stream->id(), msgBody);
}

void SimpleRTC::makeCall() {
    std::vector<std::string> msgBody;
    msgBody.push_back( "call");
    //msgBody.push_back( "media");
    msgBody.push_back("data");
    peer_->SendMessage( stream_->id(), msgBody);
}

void SimpleRTC::answerCall() {
    std::vector<std::string> msgBody;
    msgBody.push_back( "call");
    msgBody.push_back( "ok");
    peer_->SendMessage( stream_->id(), msgBody);
}

