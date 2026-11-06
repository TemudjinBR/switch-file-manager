#include "wifi_transfer.h"
#include "network/socket_runtime.h"
#include "sfm_diag.h" // SFM_DIAG_0107
#include <switch.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <dirent.h>
#include <errno.h>
#include <atomic>
#include <pthread.h>
#include <thread>
#include <string>
#include <vector>
#include <array>
#include <mutex>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <algorithm>
#include <ctime>

namespace wifi {
namespace {
#ifdef HOST_TEST
constexpr int PORT = 0;  // porta efêmera; somente loopback nos testes
#else
constexpr int PORT = 2121;
#endif
const std::string ROOT = "sdmc:/";
std::atomic<bool> active{false};
std::atomic<uint64_t> totalBytes{0}, transferBytes{0};
std::atomic<unsigned> transfersDone{0};
constexpr int MAX_CLIENTS = 4;
struct ClientSlot {
    std::atomic<bool> busy{false};
    std::atomic<int> control{-1};
    std::atomic<int> data{-1};
    pthread_t thread{};
    bool joinable = false;
};
std::array<ClientSlot, MAX_CLIENTS> clients;
std::atomic<unsigned> listingsDone{0}, connectedClients{0};
std::atomic<unsigned> changesDone{0};
std::mutex eventLock;
std::string lastEvent="Aguardando conexao";
void event(const std::string& msg){
    time_t now=time(nullptr);
    struct tm local{};
    localtime_r(&now,&local);
    char stamp[12]; strftime(stamp,sizeof(stamp),"%H:%M:%S",&local);
    std::lock_guard<std::mutex> guard(eventLock);
    lastEvent=std::string(stamp)+" - "+msg;
    sfm_diag::write("FTP", msg);
}
int listener = -1;
pthread_t worker{};
bool workerJoinable = false;
std::mutex lifecycleLock;
std::string diagnostic = "etapa=parado";
bool netReady = false;
std::string address;

#ifdef HOST_TEST
std::mutex injectedLock;
std::string injectedStage;
int injectedCode = 0;
#endif
bool before(const char* stage) {
#ifdef HOST_TEST
    std::lock_guard<std::mutex> guard(injectedLock);
    if (injectedStage == stage) { errno = injectedCode; injectedStage.clear(); return false; }
#else
    (void)stage;
#endif
    return true;
}
bool failure(std::string& error, const char* stage, int code) {
    diagnostic = std::string("etapa=") + stage + (strncmp(stage, "thread-", 7) == 0 ? "\npthread=" : "\nerrno=") + std::to_string(code) + " (" + strerror(code) + ")";
    error = diagnostic;
    sfm_diag::write("FTP", diagnostic);
    return false;
}
void join(pthread_t& thread, bool& joinable) { if (joinable) { pthread_join(thread, nullptr); joinable = false; } }
int spawn(pthread_t& thread, void* (*entry)(void*), void* arg, const char* stage) {
    if (!before(stage)) return errno;
    pthread_attr_t attr;
    int code = pthread_attr_init(&attr);
    if (code) return code;
    // Pilha explicita; buffers de transferencia permanecem no heap.
    code = pthread_attr_setstacksize(&attr, 256 * 1024);
    if (!code) code = pthread_create(&thread, &attr, entry, arg);
    pthread_attr_destroy(&attr);
    return code;
}

bool nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}
bool waitSocket(int fd, bool writing, unsigned timeoutMs = 20000) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (active) {
        fd_set set; FD_ZERO(&set); FD_SET(fd, &set);
        timeval instant{0, 0};
        int ready = select(fd + 1, writing ? nullptr : &set, writing ? &set : nullptr, nullptr, &instant);
        if (ready > 0) return true;
        if (ready < 0 && errno != EINTR) return false;
        if (timeoutMs && std::chrono::steady_clock::now() >= deadline) { errno = ETIMEDOUT; return false; }
        // Dormir fora da chamada BSD libera a sessao IPC para outros clientes e Drive.
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    errno = ECANCELED; return false;
}
bool writeAll(int fd, const char* p, size_t n) {
    while (n && active) {
        ssize_t k = send(fd, p, n, 0);
        if (k < 0 && errno == EINTR) continue;
        if (k < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) && waitSocket(fd, true)) continue;
        if (k <= 0) return false;
        p += k; n -= size_t(k);
    }
    return n == 0;
}
void reply(int fd, int code, const std::string& msg) {
    std::string s = std::to_string(code) + " " + msg + "\r\n";
    writeAll(fd, s.data(), s.size());
}
std::string joined(const std::string& dir, const std::string& file) {
    return dir + (dir.back() == '/' ? "" : "/") + file;
}
bool resolve(const std::string& cwd, const std::string& arg, std::string& out) {
    std::string logical = arg.empty() ? cwd.substr(5) :
        (arg[0] == '/' ? arg : cwd.substr(5) + (cwd == ROOT ? "" : "/") + arg);
    std::vector<std::string> names;
    std::string name;
    logical += "/";
    for (unsigned char c : logical) {
        if (c == '/') {
            if (name == "..") { if (!names.empty()) names.pop_back(); }
            else if (!name.empty() && name != ".") names.push_back(name);
            name.clear();
        } else {
            if (c == '\\' || c == ':' || c < 32 || c == 127) return false;
            name += char(c);
            if (name.size() > 255) return false;
        }
    }
    out = ROOT;
    for (size_t i = 0; i < names.size(); i++) {
        if (i) out += "/";
        out += names[i];
    }
    return out.size() < 1024;
}
std::string virtualPath(const std::string& real) {
    return real == ROOT ? "/" : "/" + real.substr(ROOT.size());
}
bool exists(const std::string& p) { struct stat st; return stat(p.c_str(), &st) == 0; }
bool isDir(const std::string& p) {
    struct stat st; return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}
bool isRegular(const std::string& p) {
    struct stat st; return stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}
bool parentExists(const std::string& p) {
    if (p == ROOT) return false;
    size_t slash = p.find_last_of('/');
    return slash != std::string::npos && isDir(p.substr(0, slash + 1));
}
std::string stamp(time_t timestamp) {
    struct tm tmval{};
    gmtime_r(&timestamp, &tmval);
    char b[24]; strftime(b, sizeof(b), "%Y%m%d%H%M%S", &tmval);
    return b;
}
std::string listing(const std::string& dir, const std::string& kind) {
    std::string result;
    DIR* dp = opendir(dir.c_str());
    if (!dp) return result;
    dirent* ent;
    while ((ent = readdir(dp))) {
        std::string name(ent->d_name);
        if (name == "." || name == "..") continue;
        std::string full = joined(dir, name);
        struct stat st{};
        if (stat(full.c_str(), &st) != 0) continue;
        if (kind == "NLST") { result += name + "\r\n"; continue; }
        if (kind == "MLSD") {
            result += std::string("type=") + (S_ISDIR(st.st_mode) ? "dir" : "file") +
                ";size=" + std::to_string((unsigned long long)st.st_size) +
                ";modify=" + stamp(st.st_mtime) + "; " + name + "\r\n";
        } else {
            struct tm tmval{}; localtime_r(&st.st_mtime, &tmval);
            char ts[24]; strftime(ts, sizeof(ts), "%b %d %H:%M", &tmval);
            result += std::string(S_ISDIR(st.st_mode) ? "drwxr-xr-x" : "-rw-r--r--") +
                " 1 owner group " + std::to_string((unsigned long long)st.st_size) +
                " " + ts + " " + name + "\r\n";
        }
    }
    closedir(dp);
    return result;
}
// File transfers and the listener are interruptible by the UI Stop button.
int passiveListener() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    #ifdef HOST_TEST
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
#else
    addr.sin_addr.s_addr = INADDR_ANY;
#endif
    addr.sin_port = 0;
    if (bind(fd, (sockaddr*)&addr, sizeof(addr)) != 0 || listen(fd, 1) != 0) {
        close(fd); return -1;
    }
    if (!nonblocking(fd)) { close(fd); return -1; }
    return fd;
}
int acceptData(int passive, int control) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (active && std::chrono::steady_clock::now() < deadline) {
        if (!waitSocket(passive, false, 15000)) break;
        sockaddr_in remote{}; socklen_t len = sizeof(remote);
        int data = accept(passive, (sockaddr*)&remote, &len);
        if (data < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) continue;
        if (data < 0) return -1;
        if (!nonblocking(data)) { close(data); return -1; }
        sockaddr_in ctrlPeer{}; len = sizeof(ctrlPeer);
        if (getpeername(control, (sockaddr*)&ctrlPeer, &len) != 0 ||
            ctrlPeer.sin_addr.s_addr != remote.sin_addr.s_addr) {
            close(data); continue;
        }
        timeval timeout{20, 0};
        setsockopt(data, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(data, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        return data;
    }
    return -1;
}
bool readLine(int fd, std::string& result) {
    result.clear();
    while (active && result.size() < 2048) {
        if (!waitSocket(fd, false, 0)) return false;
        char c;
        ssize_t got = recv(fd, &c, 1, 0);
        if (got < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) continue;
        if (got <= 0) return false;
        if (c == '\n') return true;
        if (c != '\r') result += c;
    }
    return false;
}
bool writeFile(int fd, FILE* file) {
    sfm_diag::write("FTP", "Inicio download");
    std::vector<char> buf(131072);
    size_t size;
    while (active && (size = fread(buf.data(), 1, buf.size(), file)) != 0) {
        if (!writeAll(fd, buf.data(), size)) return false;
        totalBytes += size;
        transferBytes += size;
    }
    return active && !ferror(file);
}
bool receiveFile(int fd, FILE* file) {
    sfm_diag::write("FTP", "Inicio upload");
    std::vector<char> buf(131072);
    for (;;) {
        if (!active) return false;
        ssize_t count = recv(fd, buf.data(), buf.size(), 0);
        if (count == 0) return true;
        if (count < 0 && errno == EINTR) continue;
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) && waitSocket(fd, false)) continue;
        if (count < 0) { sfm_diag::write("ERRO", "recv errno=" + std::to_string(errno)); return false; }
        size_t remaining = (size_t)count, index = 0;
        while (remaining) {
            size_t n = fwrite(buf.data() + index, 1, remaining, file);
            if (!n) { sfm_diag::write("ERRO", "fwrite errno=" + std::to_string(errno)); return false; }
            remaining -= n; index += n;
        }
        totalBytes += (unsigned long long)count;
        transferBytes += (unsigned long long)count;
    }
}
bool finalizeUpload(const std::string& dest, const std::string& part) {
    if (!exists(dest)) return rename(part.c_str(), dest.c_str()) == 0;
    if (!isRegular(dest)) return false;
    std::string backup = dest + ".sfm-backup";
    if (exists(backup) || rename(dest.c_str(), backup.c_str()) != 0) return false;
    if (rename(part.c_str(), dest.c_str()) == 0) {
        unlink(backup.c_str());
        return true;
    }
    rename(backup.c_str(), dest.c_str());
    return false;
}
void session(int fd, int slotIndex) {
    ClientSlot& slot = clients[slotIndex];
    connectedClients++;
    sockaddr_in peer{}; socklen_t peerLen=sizeof(peer);
    std::string device="Cliente FTP";
    if (getpeername(fd,(sockaddr*)&peer,&peerLen)==0) {
        char ip[INET_ADDRSTRLEN]={0};
        if (inet_ntop(AF_INET,&peer.sin_addr,ip,sizeof(ip))) device=ip;
    }
    event("Conectado: "+device);
    reply(fd, 220, "Switch File Manager FTP ready");
    std::string cwd = ROOT, pendingRename, cmd;
    int passive = -1;
    bool activeMode=false;
    sockaddr_in activeAddr{};
    uint64_t restartAt = 0;
    auto resetPasv = [&]() { if (passive >= 0) { close(passive); passive = -1; } };
    while (active && readLine(fd, cmd)) {
        size_t space = cmd.find(' ');
        std::string op = cmd.substr(0, space);
        std::transform(op.begin(), op.end(), op.begin(), [](unsigned char c) { return (char)std::toupper(c); });
        std::string arg = space == std::string::npos ? "" : cmd.substr(space + 1);
        if (op == "USER") { reply(fd, 331, "Anonymous login, enter any password"); continue; }
        if (op == "PASS") { reply(fd, 230, "Anonymous access granted"); continue; }
        if (op == "QUIT") { reply(fd, 221, "Goodbye"); break; }
        if (op == "NOOP") { reply(fd, 200, "OK"); continue; }
        if (op == "SYST") { reply(fd, 215, "UNIX Type: L8"); continue; }
        if (op == "FEAT") {
            const std::string feat = "211-Features:\r\n UTF8\r\n EPSV\r\n PASV\r\n SIZE\r\n MDTM\r\n MLSD\r\n MLST type*;size*;modify*;\r\n REST STREAM\r\n211 End\r\n";
            writeAll(fd, feat.data(), feat.size()); continue;
        }
        if (op == "OPTS" || op == "TYPE" || op == "MODE" || op == "STRU" || op == "ALLO" || op == "CLNT") {
            reply(fd, 200, "OK"); continue;
        }
        if (op == "PWD" || op == "XPWD") { reply(fd, 257, "\"" + virtualPath(cwd) + "\""); continue; }
        if (op == "REST") {
            if (arg.empty() || arg.find_first_not_of("0123456789") != std::string::npos) {
                reply(fd, 501, "Invalid offset"); continue;
            }
            char* end = nullptr;
            unsigned long long value = strtoull(arg.c_str(), &end, 10);
            if (!end || *end) { reply(fd, 501, "Invalid offset"); continue; }
            restartAt = value;
            reply(fd, 350, "Restart position accepted"); continue;
        }
        if (op == "STAT") { reply(fd, 211, "FTP active; transfers: " + std::to_string(transfersDone.load())); continue; }
        if (op == "PASV" || op == "EPSV") {
            activeMode=false;
            resetPasv();
            passive = passiveListener();
            if (passive < 0) { reply(fd, 425, "Cannot open data port"); continue; }
            sockaddr_in a{}; socklen_t len = sizeof(a);
            getsockname(passive, (sockaddr*)&a, &len);
            unsigned port = ntohs(a.sin_port);
            if (op == "EPSV") reply(fd, 229, "Entering Extended Passive Mode (|||" + std::to_string(port) + "|)");
            else {
                sockaddr_in local{}; len = sizeof(local);
                if (getsockname(fd, (sockaddr*)&local, &len) != 0) { resetPasv(); reply(fd, 425, "IP detection failed"); continue; }
                const auto* p = reinterpret_cast<const unsigned char*>(&local.sin_addr.s_addr);
                reply(fd, 227, "Entering Passive Mode (" + std::to_string(p[0]) + "," + std::to_string(p[1]) + "," + std::to_string(p[2]) + "," + std::to_string(p[3]) + "," + std::to_string(port / 256) + "," + std::to_string(port % 256) + ")");
            }
            continue;
        }
        if (op == "PORT" || op == "EPRT") {
            resetPasv(); activeMode=false;
            sockaddr_in peer{}; socklen_t peerLen=sizeof(peer);
            if(getpeername(fd,(sockaddr*)&peer,&peerLen)!=0){reply(fd,501,"Peer not found");continue;}
            sockaddr_in target{};target.sin_family=AF_INET;
            int port=0;
            if(op=="PORT"){
                unsigned a[6];char extra=0;
                if(sscanf(arg.c_str(),"%u,%u,%u,%u,%u,%u%c",&a[0],&a[1],&a[2],&a[3],&a[4],&a[5],&extra)!=6 ||
                    a[0]>255||a[1]>255||a[2]>255||a[3]>255||a[4]>255||a[5]>255){reply(fd,501,"Invalid PORT");continue;}
                char ip[32];snprintf(ip,sizeof(ip),"%u.%u.%u.%u",a[0],a[1],a[2],a[3]);
                if(inet_pton(AF_INET,ip,&target.sin_addr)!=1){reply(fd,501,"Invalid IP");continue;}
                port=(a[4]<<8)|a[5];
            }else{
                // EPRT |1|ipv4|port|, delimiter can be any printable char
                if(arg.size()<7){reply(fd,501,"Invalid EPRT");continue;}
                char delim=arg[0];size_t b=arg.find(delim,1),c=arg.find(delim,b==std::string::npos?b:b+1),d=arg.find(delim,c==std::string::npos?c:c+1);
                if(b==std::string::npos||c==std::string::npos||d!=arg.size()-1||arg.substr(1,b-1)!="1") {reply(fd,522,"IPv4 only");continue;}
                std::string ip=arg.substr(b+1,c-b-1),num=arg.substr(c+1,d-c-1);
                if(num.empty()||num.find_first_not_of("0123456789")!=std::string::npos||num.size()>5||inet_pton(AF_INET,ip.c_str(),&target.sin_addr)!=1){reply(fd,501,"Invalid EPRT");continue;}
                port=atoi(num.c_str());
            }
            if(port<1024||port>65535||target.sin_addr.s_addr!=peer.sin_addr.s_addr){reply(fd,501,"Active data target denied");continue;}
            target.sin_port=htons(port);activeAddr=target;activeMode=true;
            reply(fd,200,"Active connection configured");continue;
        }
        if (op == "ABOR") { resetPasv(); activeMode=false; reply(fd, 226, "Aborted"); continue; }
        if (op == "CDUP" || op == "XCUP") arg = "..";
        if (op == "LIST" && !arg.empty() && arg[0] == '-') arg.clear();
        std::string path;
        if (!resolve(cwd, arg, path)) { reply(fd, 550, "Invalid path"); continue; }
        if (op == "CWD" || op == "XCWD" || op == "CDUP" || op == "XCUP") {
            if (isDir(path)) { cwd = path; reply(fd, 250, "Directory changed"); }
            else reply(fd, 550, "Directory not found");
        } else if (op == "SIZE") {
            struct stat st{};
            if (stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode)) reply(fd, 213, std::to_string((unsigned long long)st.st_size));
            else reply(fd, 550, "Not a file");
        } else if (op == "MLST") {
            struct stat st{};
            if(stat(path.c_str(),&st)!=0){reply(fd,550,"Not found");continue;}
            std::string facts="type="+std::string(S_ISDIR(st.st_mode)?"dir":"file")+
                ";size="+std::to_string((unsigned long long)st.st_size)+
                ";modify="+stamp(st.st_mtime)+";";
            std::string result="250-Listing\r\n "+facts+" "+virtualPath(path)+"\r\n250 End\r\n";
            writeAll(fd,result.data(),result.size());
            continue;
        } else if (op == "MDTM") {
            struct stat st{};
            if (stat(path.c_str(), &st) == 0) reply(fd, 213, stamp(st.st_mtime));
            else reply(fd, 550, "Not found");
        } else if (op == "MKD" || op == "XMKD") {
            if (path != ROOT && parentExists(path) && mkdir(path.c_str(), 0777) == 0) { changesDone++; reply(fd, 257, "\"" + virtualPath(path) + "\" created"); }
            else reply(fd, 550, "Create failed");
        } else if (op == "RMD" || op == "XRMD") {
            if (path != ROOT && rmdir(path.c_str()) == 0) { changesDone++; reply(fd, 250, "Directory removed"); }
            else reply(fd, 550, "Remove failed");
        } else if (op == "DELE") {
            if (path != ROOT && isRegular(path) && unlink(path.c_str()) == 0) { changesDone++; reply(fd, 250, "File deleted"); }
            else reply(fd, 550, "Delete failed");
        } else if (op == "RNFR") {
            if (path != ROOT && exists(path)) { pendingRename = path; reply(fd, 350, "Ready for RNTO"); }
            else reply(fd, 550, "Not found");
        } else if (op == "RNTO") {
            if (!pendingRename.empty() && path != ROOT && !exists(path) && parentExists(path) && rename(pendingRename.c_str(), path.c_str()) == 0) { changesDone++; reply(fd, 250, "Renamed"); }
            else reply(fd, 550, "Rename failed");
            pendingRename.clear();
        } else if (op == "LIST" || op == "NLST" || op == "MLSD" || op == "RETR" || op == "STOR") {
            if (passive < 0 && !activeMode) { reply(fd, 425, "Use PASV, EPSV or PORT first"); continue; }
            bool isListing = op == "LIST" || op == "NLST" || op == "MLSD";
            if ((isListing && !isDir(path)) || (op == "RETR" && !isRegular(path)) ||
                (op == "STOR" && (path == ROOT || !parentExists(path) || (exists(path) && !isRegular(path))))) {
                resetPasv(); reply(fd, 550, "Unavailable path"); continue;
            }
            FILE* file = nullptr;
            std::string temporary;
            if (op == "RETR") {
                file = fopen(path.c_str(), "rb");
                if (file && restartAt && fseeko(file, (off_t)restartAt, SEEK_SET) != 0) { fclose(file); file = nullptr; }
            } else if (op == "STOR") {
                if (restartAt) { resetPasv(); reply(fd, 502, "Upload resume not supported"); restartAt = 0; continue; }
                temporary = path + ".sfm-part";
                int out = open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
                if (out >= 0) file = fdopen(out, "wb");
                if (out >= 0 && !file) close(out);
            }
            restartAt = 0;
            if (file) setvbuf(file, nullptr, _IOFBF, 131072);
            if (!isListing && !file) { resetPasv(); reply(fd, 550, "Cannot open file"); continue; }
            transferBytes = 0;
            if (op == "STOR") event("Upload iniciado por " + device);
            else if (op == "RETR") event("Download iniciado por " + device);
            reply(fd, 150, "Opening data connection");
            int data=-1;
            if(activeMode) {
                data=socket(AF_INET,SOCK_STREAM,0);
                if(data>=0){
                    timeval t{4,0};setsockopt(data,SOL_SOCKET,SO_SNDTIMEO,&t,sizeof(t));
                    // Nonblocking connect avoids hanging the Switch when the phone is unreachable.
                    int flags=fcntl(data,F_GETFL,0);
                    if(flags>=0)fcntl(data,F_SETFL,flags|O_NONBLOCK);
                    int rc=connect(data,(sockaddr*)&activeAddr,sizeof(activeAddr));
                    if(rc!=0&&errno==EINPROGRESS){
                        rc = waitSocket(data, true, 3000) ? 1 : -1;
                        int err=0;socklen_t sz=sizeof(err);
                        if(rc>0&&getsockopt(data,SOL_SOCKET,SO_ERROR,&err,&sz)==0&&err==0)rc=0;
                        else rc=-1;
                    }
                    if (data >= 0 && !nonblocking(data)) rc = -1;
                    if(rc!=0){close(data);data=-1;}
                    else{timeval timeout{20,0};setsockopt(data,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));setsockopt(data,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));}
                }
            }else data=acceptData(passive, fd);
            resetPasv();activeMode=false;
            // SFM_FTP_PERF_0106: buffers de dados maiores, sem alterar o protocolo.
            if (data >= 0) {
                const int socketBuffer = 262144;
                setsockopt(data, SOL_SOCKET, SO_RCVBUF, &socketBuffer, sizeof(socketBuffer));
                setsockopt(data, SOL_SOCKET, SO_SNDBUF, &socketBuffer, sizeof(socketBuffer));
            }
            slot.data=data;
            bool ok = data >= 0;
            if (ok && isListing) {
                std::string contents = listing(path, op);
                ok = writeAll(data, contents.data(), contents.size());
                if(ok){listingsDone++;event("Listagem: " + virtualPath(path));}
            } else if (ok && op == "RETR") {
                ok = writeFile(data, file);
            } else if (ok && op == "STOR") {
                ok = receiveFile(data, file);
            }
            if (data >= 0) { shutdown(data, SHUT_RDWR); close(data); }
            slot.data=-1;
            if (file) {
                if (op == "STOR" && fflush(file) != 0) ok = false;
                if (op == "STOR" && fsync(fileno(file)) != 0) ok = false;
                if (fclose(file) != 0) ok = false;
            }
            if (op == "STOR") {
                if (ok) { ok = finalizeUpload(path, temporary); if (ok) changesDone++; }
                if (!ok) unlink(temporary.c_str());
            }
            if (ok) {
                if (!isListing) { transfersDone++;event("Arquivo concluido: " + device); }
                sfm_diag::write("FTP", "Concluido: " + op + " " + path); reply(fd, 226, "Transfer complete");
            } else {event("Falha na operacao: " + device);sfm_diag::write("ERRO", "Falha: " + op + " " + path + " errno=" + std::to_string(errno)); reply(fd, 426, "Transfer interrupted or failed");}
            transferBytes = 0;
        } else {
            reply(fd, 502, "Command not supported");
        }
    }
    resetPasv();
    slot.control=-1;
    shutdown(fd, SHUT_RDWR);
    close(fd);
    connectedClients--;
    event("Desconectado: "+device);
}
void serve() {
    while (active) {
        fd_set sockets; FD_ZERO(&sockets); FD_SET(listener, &sockets);
        timeval tv{0, 0};
        int ready = select(listener + 1, &sockets, nullptr, nullptr, &tv);
        if (ready < 0 && errno != EINTR) {
            event("etapa=escutar-select errno=" + std::to_string(errno));
            active = false; break;
        }
        if (!ready) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue; }
        if (ready > 0 && active) {
            int fd = accept(listener, nullptr, nullptr);
            if(fd<0)continue;
            if (!nonblocking(fd)) { close(fd); continue; }
            int freeSlot=-1;
            for(int i=0;i<MAX_CLIENTS;i++)if(!clients[i].busy.load()){freeSlot=i;break;}
            if(freeSlot<0){
                const char* msg="421 Too many clients\r\n";
                send(fd,msg,strlen(msg),0);close(fd);continue;
            }
            ClientSlot& slot=clients[freeSlot];
            join(slot.thread, slot.joinable);
            slot.busy=true;slot.control=fd;
            int code = spawn(slot.thread, [](void* arg) -> void* {
                auto index = static_cast<size_t>(reinterpret_cast<uintptr_t>(arg));
                session(clients[index].control.load(), int(index)); clients[index].busy=false;
                return nullptr;
            }, reinterpret_cast<void*>(static_cast<uintptr_t>(freeSlot)), "thread-cliente");
            if (code) {
                slot.busy=false; slot.control=-1;
                // Publicar o diagnostico antes de avisar/fechar o cliente; a UI pode
                // consulta-lo imediatamente apos receber 421.
                event("etapa=thread-cliente pthread=" + std::to_string(code));
                const char* message="421 Thread unavailable\r\n"; send(fd,message,strlen(message),0); close(fd);
            } else slot.joinable = true;
        }
    }
}
} // anonymous namespace

bool running() { return active.load(); }
std::string url() { return address; }
uint64_t bytesTransferred() { return totalBytes.load(); }
unsigned filesCompleted() { return transfersDone.load(); }
uint64_t currentBytes() { return transferBytes.load(); }
unsigned activeClients(){return connectedClients.load();}
unsigned directoryListings(){return listingsDone.load();}
unsigned fileChanges(){return changesDone.load();}
std::string lastActivity(){std::lock_guard<std::mutex> guard(eventLock);return lastEvent;}
std::string diagnostics() { return diagnostic + "\nUltimo evento: " + lastActivity() + "\n" + sfm::net::diagnostics(); }
#ifdef HOST_TEST
void testFailure(const std::string& stage, int code) { std::lock_guard<std::mutex> guard(injectedLock); injectedStage = stage; injectedCode = code; }
#endif
bool start(std::string& error) {
    std::lock_guard<std::mutex> guard(lifecycleLock);
    error.clear();
    if (active) return true;
    // Limpar somente nossos descritores de uma execucao anterior ja encerrada.
    join(worker, workerJoinable);
    for (auto& slot : clients) join(slot.thread, slot.joinable);
    if (listener >= 0) { close(listener); listener = -1; }
    if (!netReady) {
        if (!sfm::net::acquire(sfm::net::Client::Ftp, error)) { diagnostic = error; return false; }
        netReady = true;
    }
    auto abort = [&]() {
        if (listener >= 0) { close(listener); listener = -1; }
        sfm::net::release(sfm::net::Client::Ftp); netReady = false;
        return false;
    };
    unsigned ip = 0;
    if (!sfm::net::localAddress(ip, error)) { diagnostic = error; return abort(); }
    listener = before("criar-socket") ? socket(AF_INET, SOCK_STREAM, 0) : -1;
    if (listener < 0) { failure(error, "criar-socket", errno); return abort(); }
    int yes = 1;
    if (!before("configurar-socket") || setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) != 0) {
        failure(error, "configurar-socket", errno); return abort();
    }
    sockaddr_in addr{}; addr.sin_family = AF_INET;
#ifdef HOST_TEST
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
#else
    addr.sin_addr.s_addr = INADDR_ANY;
#endif
    addr.sin_port = htons(PORT);
    if (!before("vincular-2121") || bind(listener, (sockaddr*)&addr, sizeof addr) != 0) {
        failure(error, "vincular-2121", errno); return abort();
    }
    if (!before("escutar-2121") || listen(listener, MAX_CLIENTS) != 0) {
        failure(error, "escutar-2121", errno); return abort();
    }
    sockaddr_in local{}; local.sin_addr.s_addr = ip;
    sockaddr_in bound{}; socklen_t boundSize = sizeof bound;
    if (!before("consultar-porta") || getsockname(listener, (sockaddr*)&bound, &boundSize) != 0) {
        failure(error, "consultar-porta", errno); return abort();
    }
    address = "ftp://" + std::string(inet_ntoa(local.sin_addr)) + ":" + std::to_string(ntohs(bound.sin_port));
    transfersDone = 0; totalBytes = 0; transferBytes = 0;listingsDone=0;connectedClients=0;changesDone=0;
    event("Aguardando conexao");
    if (!nonblocking(listener)) { failure(error, "listener-nao-bloqueante", errno); return abort(); }
    active = true;
    int code = spawn(worker, [](void*) -> void* { serve(); return nullptr; }, nullptr, "thread-servidor");
    if (code) { active = false; failure(error, "thread-servidor", code); return abort(); }
    workerJoinable = true;
    diagnostic = "etapa=ativo porta=2121";
    return true;
}
void stop() {
    std::lock_guard<std::mutex> guard(lifecycleLock);
    active = false;
    join(worker, workerJoinable);
    // Descritores nao bloqueantes pertencem ao worker. Evitar shutdown pela UI:
    // o numero pode ter sido fechado/reutilizado por outra conexao concorrente.
    for(auto& slot:clients) join(slot.thread, slot.joinable);
    if (listener >= 0) { close(listener); listener = -1; }
    if (netReady) { sfm::net::release(sfm::net::Client::Ftp); netReady = false; }
    address.clear();
}
} // namespace wifi
