#include "cloud/download_manager.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cerrno>
#include <unistd.h>
using namespace gd::downloads;
struct Fake : Transport {
    std::string mode; unsigned fetched = 0, inspections = 0; bool changed = false;
    std::string data() { std::string value(250000, 0); for (size_t i=0;i<value.size();++i) value[i] = char((i * 17 + (changed ? 7 : 3)) % 251); return value; }
    bool inspect(const std::string& id, Remote& remote, Error& error) override {
        ++inspections;
        if (mode == "offline" || (mode == "cut-offline" && fetched)) { error = {Kind::Network, "offline", 100, "Ficticio"}; return false; }
        remote = {id, changed ? "2" : "1", "", mode == "large-prefix" ? uint64_t(2) << 30 : data().size()}; return true;
    }
    Fetch fetch(const Remote&, uint64_t start, uint64_t last, const Writer& write, Error& error) override {
        ++fetched; std::cout << "range=" << start << '-' << last << std::endl;
        if (mode == "large-prefix") { error={Kind::Network,"queda-ficticia",104,"Ficticio"}; Fetch result; return result; }
        Fetch result; result.http = 206; result.headersValid = true; result.mediaTag = changed ? "tag-2" : "tag-1";
        if (mode == "bad-range") { result.headersValid=false; result.retryable=false; error={Kind::Protocol,"intervalo-invalido",206,"Ficticio"}; return result; }
        if (mode == "no-range" && start) { result.http=200;result.headersValid=false;result.restart=true;return result; }
        auto value = data();
        uint64_t count = last-start+1;
        if (mode == "no-range" && fetched > 1 && start == 0) { count = value.size(); result.http=200; }
        bool interrupt = (mode == "cut" && fetched <= 2) || (mode == "no-range" && fetched == 1) || mode == "always-cut" || mode == "cut-offline";
        if (interrupt) count = std::min<uint64_t>(count, 17000);
        uint64_t pos=0;
        while(pos<count) {
            auto n=size_t(std::min<uint64_t>(4096,count-pos));
            if (!write(reinterpret_cast<const uint8_t*>(value.data()+start+pos),n)) return result;
            pos+=n;
        }
        if (mode == "change" && fetched == 1) changed=true;
        if (interrupt) { error={Kind::Network,"conexao-caiu",104,"Ficticio"}; return result; }
        result.complete=true;return result;
    }
};
struct UI : Observer {
    std::string mode, destination; Status status; unsigned paused = 0;
    void update(const Status& next) override { status=next; if (mode=="concurrent" && next.state==State::Finalizing) { std::ofstream out(destination); out<<"personal-concurrent"; } if (next.state==State::Paused) ++paused; std::cout << "state=" << label(next.state) << " offset=" << next.current << " attempt=" << next.attempt << std::endl; }
    bool restart(const std::string&) override { return mode != "decline-restart"; }
    Action action() override {
        if (mode=="cancel" && status.current>=10000) return Action::Cancel;
        if (mode=="pause" && status.current>=10000 && paused<3) return Action::Pause;
        return Action::Continue;
    }
};
int main(int argc,char**argv) {
    if(argc<5)return 2;
    std::string command=argv[1],area=argv[2],dest=argv[3],mode=argv[4];
    if (argc>5) {
        std::string hook=argv[5];
        if(hook.rfind("crash=",0)==0)testCrash(hook.substr(6));
        if(hook.rfind("fail=",0)==0)testFailure(hook.substr(5),hook.substr(5)=="espaco-livre"?ENOSPC:5);
    }
    Error error;
    if(command=="list") {
        std::vector<Job>jobs;unsigned invalid=0;
        bool ok=list(area,"fixture-owner",jobs,invalid,error);
        for(auto&job:jobs)std::cout<<"job="<<job.id<<" offset="<<job.offset<<" state="<<label(job.state)<<std::endl;
        std::cout<<"invalid="<<invalid<<std::endl;return ok?0:1;
    }
    Fake transport;transport.mode=mode; UI ui;ui.mode=mode;ui.destination=dest;
    Policy policy;policy.baseDelayMs=1;policy.maxDelayMs=4;policy.chunkBytes=65536;policy.maxRetries=mode=="large-prefix"?0:3;
    bool ok=run(area,"fixture-owner",command=="other"?"other-id":"fixture-id",dest,transport,&ui,error,policy);
    if(!ok)std::cerr<<"stage="<<error.stage<<" code="<<error.code<<" "<<error.message<<std::endl;
    return ok?0:1;
}
