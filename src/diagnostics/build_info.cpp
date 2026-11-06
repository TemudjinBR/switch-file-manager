#include "diagnostics/build_info.h"
#include "version.h"
#include <cstdint>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

namespace sfm::build {
namespace {
const char marker[] = "SFM_BUILD:" SFM_BUILD_ID;
uint32_t u32(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24; }
uint64_t u64(const uint8_t* p) { return uint64_t(u32(p)) | uint64_t(u32(p+4))<<32; }
bool readAt(FILE* file, uint64_t offset, void* out, size_t size) {
    return offset <= INT64_MAX && fseeko(file, off_t(offset), SEEK_SET) == 0 && fread(out, 1, size, file) == size;
}
bool candidate(const std::string& path, uint64_t size, Copy& copy, uint64_t& budget, const std::function<bool()>& keepGoing, bool& cancelled) {
    if (size < 0x80 || size > 32u*1024*1024) return false;
    FILE* file = fopen(path.c_str(), "rb"); if (!file) return false;
    uint8_t header[0x80]{}, assets[0x38]{}; char title[0x200]{}; char version[16]{};
    bool ok = readAt(file, 0, header, sizeof header) && memcmp(header+0x10,"NRO0",4)==0;
    uint64_t exe = u32(header+0x18);
    ok = ok && exe >= 0x80 && exe <= size && readAt(file, exe, assets, sizeof assets) && memcmp(assets,"ASET",4)==0;
    uint64_t offset = u64(assets+24), length=u64(assets+32);
    ok = ok && length==0x4000 && offset>=0x38 && offset<=size-exe && length<=size-exe-offset;
    ok = ok && readAt(file,exe+offset,title,sizeof title) && strnlen(title,sizeof title)==strlen("Switch File Manager") && strcmp(title,"Switch File Manager")==0;
    ok = ok && readAt(file,exe+offset+0x3060,version,sizeof version) && memchr(version,0,sizeof version);
    if (ok) {
        copy = {path, version, "indisponivel"};
        std::string previous; uint8_t buffer[8192];
        for (uint64_t pos=0;pos<exe && budget;) {
            if (keepGoing && !keepGoing()) { cancelled = true; break; }
            size_t count=size_t(std::min<uint64_t>(sizeof buffer,std::min<uint64_t>(exe-pos,budget)));
            if (!readAt(file,pos,buffer,count)) break;
            pos+=count;budget-=count;
            std::string text=previous+std::string(reinterpret_cast<char*>(buffer),count);
            size_t start=0;
            while((start=text.find("SFM_BUILD:v",start))!=std::string::npos) {
                size_t end=text.find('\0',start);
                if(end!=std::string::npos && end-start<80) {
                    std::string id=text.substr(start+10,end-start-10);
                    size_t dash=id.find('-');
                    if(dash!=std::string::npos && id.size()-dash-1==12 && id.find_first_not_of("0123456789abcdef",dash+1)==std::string::npos) {copy.build=id;break;}
                }
                ++start;
            }
            if(copy.build!="indisponivel")break;
            previous=text.substr(text.size()>100?text.size()-100:0);
        }
    }
    fclose(file); return ok;
}
}
const char* identity() { return marker; }
Scan inspect(const std::string& root, const std::function<bool()>& keepGoing) {
    Scan out; uint64_t budget=32u*1024*1024;
    std::function<void(const std::string&,unsigned)> visit;
    visit = [&](const std::string& directory,unsigned depth) {
        DIR* dir=opendir(directory.c_str());if(!dir)return;
        while(auto* entry=readdir(dir)) {
            if (keepGoing && !keepGoing()) {out.cancelled=true;break;}
            if (++out.examined>8000 || out.copies.size()>=64 || !budget) {out.limited=true;break;}
            std::string name=entry->d_name;if(name=="."||name=="..")continue;
            std::string path=directory+"/"+name;struct stat info{};
            if(lstat(path.c_str(),&info)!=0)continue;
            if(S_ISDIR(info.st_mode)) {if(depth<5)visit(path,depth+1);else out.limited=true;}
            else if(S_ISREG(info.st_mode)&&name.size()>4&&name.substr(name.size()-4)==".nro") {
                Copy value;if(candidate(path,info.st_size,value,budget,keepGoing,out.cancelled))out.copies.push_back(value);
                if (!budget) out.limited = true;
            }
            if(out.cancelled||out.examined>8000||!budget)break;
        }
        closedir(dir);
    };
    visit(root,0);return out;
}
}
