#pragma once
#include <cstdio>
#include <ctime>
#include <mutex>
#include <string>
#include <sys/stat.h>
namespace sfm_diag {
inline void write(const char* category,const std::string& msg) {
    static std::mutex lock;
    std::lock_guard<std::mutex> guard(lock);
    mkdir("sdmc:/switch",0777);
    mkdir("sdmc:/switch/SwitchFileManager",0777);
    mkdir("sdmc:/switch/SwitchFileManager/logs",0777);
    FILE* f=fopen("sdmc:/switch/SwitchFileManager/logs/sfm.log","ab");
    if (!f) return;
    time_t now=time(nullptr);
    struct tm t{};
    localtime_r(&now,&t);
    char stamp[32]={0};
    strftime(stamp,sizeof(stamp),"%Y-%m-%d %H:%M:%S",&t);
    fprintf(f,"[%s] [%s] %s\n",stamp,category,msg.c_str());
    fflush(f);
    fclose(f);
}
}
