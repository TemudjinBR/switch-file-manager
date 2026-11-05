#include "wifi_transfer.h"
#include <switch.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/select.h>
#include <errno.h>
#include <atomic>
#include <thread>
#include <string>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <algorithm>

namespace wifi {
static constexpr int PORT=8765;
static const char* ROOT="sdmc:/switch/SwitchFileManager/WiFi";
static std::atomic<bool> active{false};
static int listener=-1;
static std::thread worker;
static std::string token, address;
static bool netReady=false;
static bool pathExists(const std::string& p){struct stat st;return stat(p.c_str(),&st)==0;}
static bool sendAll(int fd,const char* p,size_t n) {
    while(n){ ssize_t k=send(fd,p,n,0); if(k<=0)return false; p+=k;n-=size_t(k); }return true;
}
static void response(int fd,int code,const char* type,const std::string& data){
    std::string h="HTTP/1.1 "+std::to_string(code)+(code==200?" OK":" Error")+"\r\nContent-Type: "+type+"\r\nContent-Length: "+std::to_string(data.size())+"\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\n\r\n";
    sendAll(fd,h.data(),h.size());sendAll(fd,data.data(),data.size());
}
static std::string encode(const std::string& s){
    const char* hex="0123456789ABCDEF";std::string o;
    for(unsigned char c:s){if(std::isalnum(c)||c=='-'||c=='_'||c=='.')o+=char(c);else{o+='%';o+=hex[c>>4];o+=hex[c&15];}}return o;
}
static std::string decode(const std::string& s){
    std::string o;
    for(size_t i=0;i<s.size();++i){if(s[i]=='%'&&i+2<s.size()&&std::isxdigit((unsigned char)s[i+1])&&std::isxdigit((unsigned char)s[i+2])){
        char v[3]={s[i+1],s[i+2],0};o+=char(strtol(v,nullptr,16));i+=2;
    }else if(s[i]=='+')o+=' ';else o+=s[i];}return o;
}
static bool validName(const std::string& n){
    if(n.empty()||n.size()>180||n=="."||n==".."||n[0]=='.')return false;
    for(unsigned char c:n)if(c=='/'||c=='\\'||c==':'||c<32||c==127)return false;
    return true;
}
static std::string param(const std::string& query,const std::string& key){
    std::string needle=key+"=";size_t pos=0;
    while(pos<query.size()){size_t end=query.find('&',pos);if(end==std::string::npos)end=query.size();
        if(query.compare(pos,needle.size(),needle)==0)return decode(query.substr(pos+needle.size(),end-pos-needle.size()));pos=end+1;}
    return "";
}
static std::string htmlEscape(const std::string& s){
    std::string o;for(char c:s){switch(c){case '&':o+="&amp;";break;case '<':o+="&lt;";break;case '>':o+="&gt;";break;case '"':o+="&quot;";break;case '\'':o+="&#39;";break;default:o+=c;}}return o;
}
static void client(int fd){
    timeval tv{15,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof(tv));
    std::string req;char buf[16384];size_t headerEnd=std::string::npos;
    while(req.size()<32768){ssize_t n=recv(fd,buf,sizeof(buf),0);if(n<=0)return;req.append(buf,size_t(n));headerEnd=req.find("\r\n\r\n");if(headerEnd!=std::string::npos)break;}
    if(headerEnd==std::string::npos){response(fd,400,"text/plain","Cabeçalho inválido");return;}
    size_t lineEnd=req.find("\r\n");std::string first=req.substr(0,lineEnd);size_t a=first.find(' '),b=first.find(' ',a+1);
    if(a==std::string::npos||b==std::string::npos){response(fd,400,"text/plain","Requisição inválida");return;}
    std::string method=first.substr(0,a),target=first.substr(a+1,b-a-1);
    size_t q=target.find('?');std::string path=target.substr(0,q),query=q==std::string::npos?"":target.substr(q+1);
    if(method=="GET"&&path=="/"&&param(query,"key")!=token){
        const std::string login = "<!doctype html><html lang='pt-br'><meta charset='utf-8'>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<title>Switch File Manager - Acesso</title>"
            "<style>body{font:18px system-ui;background:#111827;color:white;max-width:440px;"
            "margin:12vh auto;padding:24px}input,button{box-sizing:border-box;width:100%;"
            "padding:14px;margin:10px 0;border-radius:9px;font-size:17px}"
            "button{background:#248ad1;color:white;border:0}</style>"
            "<h2>Switch File Manager</h2><p>Digite o código mostrado no Switch.</p>"
            "<form method='GET' action='/'><input name='key' aria-label='Código de acesso' "
            "autocomplete='off' autocapitalize='off' spellcheck='false' required>"
            "<button type='submit'>Entrar</button></form></html>";
        response(fd,200,"text/html; charset=utf-8",login);return;
    }
    if(param(query,"key")!=token){response(fd,403,"text/plain","Código inválido");return;}
    if(method=="GET"&&path=="/"){
        std::string h="<!doctype html><html lang=pt-br><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'><title>Switch File Manager</title><style>body{font:16px system-ui;background:#111827;color:white;max-width:650px;margin:30px auto;padding:16px}button,input{padding:12px;margin:6px}a{color:#8bd4ff}li{padding:8px}#status{white-space:pre-wrap}</style><h2>Switch File Manager — Wi-Fi</h2><p>Arquivos na pasta compartilhada do cartão SD.</p><input type=file id=f multiple><button onclick=upload()>Enviar arquivos</button><p id=status></p><h3>Baixar arquivos</h3><ul>";
        DIR* d=opendir(ROOT);if(d){dirent* e;while((e=readdir(d))){std::string n=e->d_name;if(!validName(n))continue;struct stat st;std::string p=std::string(ROOT)+"/"+n;if(stat(p.c_str(),&st)||!S_ISREG(st.st_mode))continue;h+="<li><a href='/download?key="+token+"&name="+encode(n)+"'>"+htmlEscape(n)+"</a> ("+std::to_string((unsigned long long)st.st_size)+" bytes)</li>";}closedir(d);}
        h+="</ul><script>const key='"+token+"';async function upload(){const files=document.getElementById('f').files,st=document.getElementById('status');for(const f of files){st.textContent='Enviando '+f.name;try{const r=await fetch('/upload?key='+key+'&name='+encodeURIComponent(f.name),{method:'POST',headers:{'Content-Type':'application/octet-stream'},body:f});if(!r.ok)throw Error(await r.text());st.textContent+=' — concluído';}catch(e){st.textContent+=' — erro: '+e.message;return}}location.reload()}</script></html>";
        response(fd,200,"text/html; charset=utf-8",h);return;
    }
    std::string name=param(query,"name");if(!validName(name)){response(fd,400,"text/plain","Nome de arquivo inválido");return;}
    std::string dest=std::string(ROOT)+"/"+name;
    if(method=="GET"&&path=="/download"){
        struct stat st;if(stat(dest.c_str(),&st)||!S_ISREG(st.st_mode)){response(fd,404,"text/plain","Arquivo não encontrado");return;}
        FILE* f=fopen(dest.c_str(),"rb");if(!f){response(fd,500,"text/plain","Erro de leitura");return;}
        std::string h="HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Disposition: attachment; filename*=UTF-8''"+encode(name)+"\r\nContent-Length: "+std::to_string((unsigned long long)st.st_size)+"\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n";
        bool ok=sendAll(fd,h.data(),h.size());size_t n;while(ok&&(n=fread(buf,1,sizeof(buf),f))>0)ok=sendAll(fd,buf,n);fclose(f);return;
    }
    if(method=="POST"&&path=="/upload"){
        if(pathExists(dest)){response(fd,409,"text/plain","Já existe arquivo com esse nome");return;}
        size_t p=req.find("\r\nContent-Length:");if(p==std::string::npos){ // case-insensitive header scan
            std::string hdr=req.substr(0,headerEnd);std::transform(hdr.begin(),hdr.end(),hdr.begin(),[](unsigned char c){return std::tolower(c);});p=hdr.find("\r\ncontent-length:");
        }
        if(p==std::string::npos){response(fd,411,"text/plain","Content-Length obrigatório");return;}
        p=req.find(':',p)+1;size_t end=req.find("\r\n",p);std::string len=req.substr(p,end-p);
        char* tail=nullptr;unsigned long long total=strtoull(len.c_str(),&tail,10);
        if(total>1024ULL*1024*1024||total==0||tail==len.c_str()||(*tail && !std::isspace((unsigned char)*tail))){response(fd,413,"text/plain","Tamanho inválido ou maior que 1 GB");return;}
        std::string part=dest+".sfm-part";
        int out=open(part.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);if(out<0){response(fd,409,"text/plain","Destino ocupado");return;}
        size_t received=req.size()-(headerEnd+4);bool ok=received<=total;
        if(ok&&received)ok=write(out,req.data()+headerEnd+4,received)==(ssize_t)received;
        unsigned long long remaining=total-received;
        while(ok&&remaining&&active){ssize_t n=recv(fd,buf,(size_t)std::min<unsigned long long>(sizeof(buf),remaining),0);if(n<=0){ok=false;break;}ok=write(out,buf,(size_t)n)==n;remaining-=n;}
        if(fsync(out)!=0)ok=false;if(close(out)!=0)ok=false;
        if(ok&&remaining==0&&!pathExists(dest)&&rename(part.c_str(),dest.c_str())==0){response(fd,200,"text/plain","Enviado");return;}
        unlink(part.c_str());response(fd,500,"text/plain","Envio incompleto ou falha de gravação");return;
    }
    response(fd,404,"text/plain","Rota inexistente");
}
static void serve(){
    while(active){fd_set set;FD_ZERO(&set);FD_SET(listener,&set);timeval t{0,200000};int ready=select(listener+1,&set,nullptr,nullptr,&t);
        if(ready>0){int fd=accept(listener,nullptr,nullptr);if(fd>=0){client(fd);close(fd);}}
    }
}
bool running(){return active.load();}
std::string url(){return address;}
bool start(std::string& error){
    if(active)return true;
    if(!netReady){Result r=socketInitializeDefault();if(R_FAILED(r)){error="Falha ao inicializar a rede";return false;}netReady=true;}
    mkdir("sdmc:/switch/SwitchFileManager",0777);mkdir(ROOT,0777);
    listener=socket(AF_INET,SOCK_STREAM,0);if(listener<0){error="Não foi possível abrir o servidor";return false;}
    int one=1;setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one));sockaddr_in sa{};sa.sin_family=AF_INET;sa.sin_port=htons(PORT);sa.sin_addr.s_addr=INADDR_ANY;
    if(bind(listener,(sockaddr*)&sa,sizeof(sa))!=0||listen(listener,2)!=0){close(listener);listener=-1;error="Porta 8765 indisponível";return false;}
    unsigned char randomBytes[12];randomGet(randomBytes,sizeof(randomBytes));static const char* hex="0123456789abcdef";token.clear();for(auto b:randomBytes){token+=hex[b>>4];token+=hex[b&15];}
    int probe=socket(AF_INET,SOCK_DGRAM,0);
    sockaddr_in local{};
    bool ipOk=false;
    if(probe>=0){
        sockaddr_in remote{};remote.sin_family=AF_INET;remote.sin_port=htons(53);
        inet_pton(AF_INET,"1.1.1.1",&remote.sin_addr);
        if(connect(probe,(sockaddr*)&remote,sizeof(remote))==0){
            socklen_t sl=sizeof(local);
            ipOk=getsockname(probe,(sockaddr*)&local,&sl)==0 &&
                local.sin_addr.s_addr!=INADDR_ANY;
        }
        close(probe);
    }
    if(!ipOk){close(listener);listener=-1;error="Não foi possível identificar o IP local. Verifique o Wi-Fi.";return false;}
    address="http://"+std::string(inet_ntoa(local.sin_addr))+":"+std::to_string(PORT)+"/?key="+token;
    active=true;worker=std::thread(serve);return true;
}
void stop(){
    if(active){active=false;if(worker.joinable())worker.join();}
    if(listener>=0){close(listener);listener=-1;}
    if(netReady){socketExit();netReady=false;}
}
}
