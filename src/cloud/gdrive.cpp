// Módulo de rede + Google Drive
// HTTPS: serviço SSL nativo do Switch (libnx). Em HOST_TEST usa sockets simples para testes no PC.

#ifndef HOST_TEST
#include <switch.h>
#endif
#include "gdrive.h"
#include "security/oauth.h"
#include <set>
#include "network/socket_runtime.h"
#include "core/paths.h"
#include "cloud/credential_store.h"
#include "sfm_diag.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <poll.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <algorithm>

namespace gd {

#ifdef HOST_TEST
int testPort = 0;
std::string testDir = "/tmp/gdtest";
int testLoginPort = 8080;
#endif

static std::string g_err;
static ErrorSource g_errorSource = ErrorSource::None;
static ConnectionState g_connectionState = ConnectionState::AuthorizationRequired;
std::string lastError() { return g_err; }
ErrorSource lastErrorSource() { return g_errorSource; }
static void setErr(const std::string& e, ErrorSource source = ErrorSource::Other) {
    g_err = e; g_errorSource = source;
    if (source == ErrorSource::Storage) g_connectionState = ConnectionState::StorageError;
    if (source == ErrorSource::Network) g_connectionState = ConnectionState::NetworkError;
    if (source == ErrorSource::Google || source == ErrorSource::Protocol) g_connectionState = ConnectionState::AuthorizationRequired;
}
static void clearError() { g_err.clear(); g_errorSource = ErrorSource::None; }

// ----------------------------------------------------------------- utilidades

static std::string urlEncode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string r;
    for (unsigned char c : s) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') r += (char)c;
        else { r += '%'; r += hex[c >> 4]; r += hex[c & 15]; }
    }
    return r;
}

static int hexv(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static std::string urlDecode(const std::string& s) {
    std::string r;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '+') r += ' ';
        else if (s[i] == '%' && i + 2 < s.size() + 0 && hexv(s[i + 1]) >= 0 && hexv(s[i + 2]) >= 0) {
            r += (char)(hexv(s[i + 1]) * 16 + hexv(s[i + 2]));
            i += 2;
        } else r += s[i];
    }
    return r;
}

static std::string lowerStr(std::string s) {
    for (auto& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

static std::string configDir() {
#ifdef HOST_TEST
    return testDir;
#else
    return sfm::paths::kLegacyConfigDir;
#endif
}

static void storageError(const std::string& message, const persist::Error& error) {
    sfm_diag::write("DRIVE_SD", "etapa=" + error.stage + " errno=" + std::to_string(error.code));
    setErr("SD: " + persist::describe(error) + ". " + message + " Dados anteriores preservados.", ErrorSource::Storage);
}

static bool ensureConfigDir() {
    persist::Error error;
    if (persist::ensureDirectory(configDir(), error)) return true;
    storageError("Falha ao preparar o diretório de login", error);
    return false;
}

static bool readFile(persist::File file, std::string& out, bool missingAllowed = true) {
    persist::Error error;
    if (persist::load(configDir(), file, out, error)) return true;
    if (error.code != ENOENT || !missingAllowed) storageError("Falha ao ler os dados salvos", error);
    return false;
}

static bool writeFile(persist::File file, const std::string& data) {
    persist::Error error;
    if (persist::save(configDir(), file, data, error)) return true;
    const char* message = file == persist::File::Token ? "Falha ao salvar login no cartão SD." :
                          file == persist::File::Credentials ? "Falha ao salvar credencial no cartão SD." :
                          "Falha ao salvar estado de conexão no cartão SD.";
    storageError(message, error);
    return false;
}

// ----------------------------------------------------------------- JSON mínimo

struct JV {
    enum T { Null, Bool, Num, Str, Arr, Obj } t = Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<JV> a;
    std::vector<std::pair<std::string, JV>> o;

    const JV* get(const char* k) const {
        for (auto& kv : o) if (kv.first == k) return &kv.second;
        return nullptr;
    }
    std::string str(const char* k) const {
        const JV* v = get(k);
        if (!v) return "";
        if (v->t == Str) return v->s;
        if (v->t == Num) { char b[32]; snprintf(b, sizeof b, "%.0f", v->n); return b; }
        return "";
    }
};

struct JP {
    const char* p;
    const char* e;
    int depth = 0;

    void ws() { while (p < e && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++; }

    static void putUtf8(std::string& s, uint32_t cp) {
        if (cp < 0x80) s += (char)cp;
        else if (cp < 0x800) { s += (char)(0xC0 | (cp >> 6)); s += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { s += (char)(0xE0 | (cp >> 12)); s += (char)(0x80 | ((cp >> 6) & 0x3F)); s += (char)(0x80 | (cp & 0x3F)); }
        else { s += (char)(0xF0 | (cp >> 18)); s += (char)(0x80 | ((cp >> 12) & 0x3F)); s += (char)(0x80 | ((cp >> 6) & 0x3F)); s += (char)(0x80 | (cp & 0x3F)); }
    }

    bool hex4(uint32_t& v) {
        if (e - p < 4) return false;
        v = 0;
        for (int i = 0; i < 4; i++) {
            int h = hexv(p[i]);
            if (h < 0) return false;
            v = v * 16 + h;
        }
        p += 4;
        return true;
    }

    bool str(std::string& out) {
        if (p >= e || *p != '"') return false;
        p++;
        out.clear();
        while (p < e && *p != '"') {
            if (*p == '\\') {
                p++;
                if (p >= e) return false;
                char c = *p++;
                switch (c) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'u': {
                        uint32_t v;
                        if (!hex4(v)) return false;
                        if (v >= 0xD800 && v < 0xDC00 && e - p >= 6 && p[0] == '\\' && p[1] == 'u') {
                            p += 2;
                            uint32_t lo;
                            if (!hex4(lo)) return false;
                            v = 0x10000 + ((v - 0xD800) << 10) + (lo - 0xDC00);
                        }
                        putUtf8(out, v);
                        break;
                    }
                    default: out += c;
                }
            } else out += *p++;
        }
        if (p >= e) return false;
        p++;
        return true;
    }

    bool val(JV& v) {
        ws();
        if (p >= e || depth > 40) return false;
        if (*p == '{') {
            depth++;
            p++;
            v.t = JV::Obj;
            ws();
            if (p < e && *p == '}') { p++; depth--; return true; }
            for (;;) {
                ws();
                std::string k;
                if (!str(k)) return false;
                ws();
                if (p >= e || *p != ':') return false;
                p++;
                JV c;
                if (!val(c)) return false;
                v.o.emplace_back(k, std::move(c));
                ws();
                if (p < e && *p == ',') { p++; continue; }
                if (p < e && *p == '}') { p++; break; }
                return false;
            }
            depth--;
            return true;
        }
        if (*p == '[') {
            depth++;
            p++;
            v.t = JV::Arr;
            ws();
            if (p < e && *p == ']') { p++; depth--; return true; }
            for (;;) {
                JV c;
                if (!val(c)) return false;
                v.a.push_back(std::move(c));
                ws();
                if (p < e && *p == ',') { p++; continue; }
                if (p < e && *p == ']') { p++; break; }
                return false;
            }
            depth--;
            return true;
        }
        if (*p == '"') { v.t = JV::Str; return str(v.s); }
        if (e - p >= 4 && !strncmp(p, "true", 4)) { v.t = JV::Bool; v.b = true; p += 4; return true; }
        if (e - p >= 5 && !strncmp(p, "false", 5)) { v.t = JV::Bool; v.b = false; p += 5; return true; }
        if (e - p >= 4 && !strncmp(p, "null", 4)) { v.t = JV::Null; p += 4; return true; }
        char* endp;
        std::string tmp(p, std::min<size_t>(e - p, 40));
        v.n = strtod(tmp.c_str(), &endp);
        if (endp == tmp.c_str()) return false;
        v.t = JV::Num;
        p += (endp - tmp.c_str());
        return true;
    }
};

static bool jparse(const std::string& s, JV& out) {
    JP jp;
    jp.p = s.data();
    jp.e = s.data() + s.size();
    out = JV();
    return jp.val(out);
}

static std::string jescape(const std::string& s) {
    std::string r;
    for (unsigned char c : s) {
        switch (c) {
            case '"': r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n"; break;
            case '\r': r += "\\r"; break;
            case '\t': r += "\\t"; break;
            default:
                if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); r += b; }
                else r += (char)c;
        }
    }
    return r;
}

// ----------------------------------------------------------------- conexão

static bool g_netUp = false;
#ifndef HOST_TEST
static bool g_sslUp = false;
static SslContext g_sslCtx;
#endif

bool netInit() {
    if (!g_netUp) {
        std::string error;
        if (!sfm::net::acquire(sfm::net::Client::Drive, error)) { setErr(error, ErrorSource::Network); return false; }
        g_netUp = true;
    }
#ifndef HOST_TEST
    if (!g_sslUp) {
        Result rc = sslInitialize(3);
        if (R_SUCCEEDED(rc)) {
            rc = sslCreateContext(&g_sslCtx, SslVersion_Auto);
            if (R_FAILED(rc)) sslExit();
        }
        if (R_FAILED(rc)) {
            char code[32]; snprintf(code, sizeof code, "0x%08X", static_cast<unsigned>(rc));
            setErr(std::string("Rede: falha ao iniciar SSL. Result=") + code, ErrorSource::Network);
            sfm_diag::write("REDE", std::string("etapa=ssl-inicializar Result=") + code);
            sfm::net::release(sfm::net::Client::Drive); g_netUp = false;
            return false;
        }
        g_sslUp = true;
    }
#endif
    return true;
}

void netExit() {
#ifndef HOST_TEST
    if (g_sslUp) { sslContextClose(&g_sslCtx); sslExit(); g_sslUp = false; }
#endif
    if (g_netUp) { sfm::net::release(sfm::net::Client::Drive); g_netUp = false; }
}

struct Resp {
    int status = 0;
    std::map<std::string, std::string> h;
};

struct Sink {
    virtual void onHeaders(const Resp&) {}
    virtual bool put(const uint8_t* p, size_t n) = 0;
    virtual ~Sink() {}
};

struct StrSink : Sink {
    std::string s;
    void onHeaders(const Resp&) override { s.clear(); }  // uma resposta por tentativa, inclusive após HTTP 401
    bool put(const uint8_t* p, size_t n) override {
        if (s.size() + n > (64u << 20)) return false;
        s.append((const char*)p, n);
        return true;
    }
};

struct Conn {
    int fd = -1;
#ifndef HOST_TEST
    SslConnection ssl;
    int sslfd = -1;
    bool sslOpen = false;
#endif
    static const size_t BS = 65536;
    uint8_t* buf = nullptr;
    size_t bpos = 0, blen = 0;

    Conn() { buf = (uint8_t*)malloc(BS); }
    ~Conn() { closeAll(); free(buf); }

    bool connectTo(const std::string& host, int port, bool tls) {
        addrinfo hints;
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* res = nullptr;
        char ps[16];
        snprintf(ps, sizeof ps, "%d", port);
        if (getaddrinfo(host.c_str(), ps, &hints, &res) != 0 || !res) {
            setErr("Sem conexão com a internet (DNS).");
            return false;
        }
        fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (fd < 0) { freeaddrinfo(res); setErr("Falha ao criar o socket."); return false; }
        struct timeval tv;
        tv.tv_sec = 30;
        tv.tv_usec = 0;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
        if (connect(fd, res->ai_addr, res->ai_addrlen) != 0) {
            freeaddrinfo(res);
            closeAll();
            setErr("Não foi possível conectar ao servidor.");
            return false;
        }
        freeaddrinfo(res);
#ifdef HOST_TEST
        (void)tls;
        return true;
#else
        if (!tls) return true;
        Result rc = sslContextCreateConnection(&g_sslCtx, &ssl);
        if (R_FAILED(rc)) { setErr("Falha ao abrir a conexão segura."); closeAll(); return false; }
        sslOpen = true;
        int o = socketSslConnectionSetSocketDescriptor(&ssl, fd);
        if (o == -1 && errno != ENOENT) { setErr("Falha ao ligar o socket ao SSL."); closeAll(); return false; }
        sslfd = o;
        rc = sslConnectionSetHostName(&ssl, host.c_str(), (u32)host.size());
        if (R_FAILED(rc)) { setErr("Falha ao configurar o servidor seguro."); closeAll(); return false; }
        u32 sz = 0, certs = 0;
        rc = sslConnectionDoHandshake(&ssl, &sz, &certs, NULL, 0);
        if (R_FAILED(rc)) {
            char b[96];
            snprintf(b, sizeof b, "Falha na conexão segura (código 0x%X). Verifique data/hora e internet.", (unsigned)rc);
            setErr(b);
            closeAll();
            return false;
        }
        usingTls = true;
        return true;
#endif
    }

    bool usingTls = false;

    void closeAll() {
#ifndef HOST_TEST
        if (sslOpen) { sslConnectionClose(&ssl); sslOpen = false; }
        if (sslfd >= 0) { close(sslfd); sslfd = -1; }
#endif
        if (fd >= 0) { close(fd); fd = -1; }
        usingTls = false;
    }

    long rawRead(void* b, size_t n) {
#ifdef HOST_TEST
        return recv(fd, b, n, 0);
#else
        if (usingTls) {
            u32 out = 0;
            Result rc = sslConnectionRead(&ssl, b, (u32)n, &out);
            if (R_FAILED(rc)) return -1;
            return (long)out;
        }
        return recv(fd, b, n, 0);
#endif
    }

    bool rawWriteAll(const void* b, size_t n) {
        const uint8_t* p = (const uint8_t*)b;
        while (n > 0) {
            long w;
#ifdef HOST_TEST
            w = send(fd, p, n, 0);
#else
            if (usingTls) {
                u32 out = 0;
                Result rc = sslConnectionWrite(&ssl, p, (u32)std::min<size_t>(n, 1 << 20), &out);
                w = R_FAILED(rc) ? -1 : (long)out;
            } else w = send(fd, p, n, 0);
#endif
            if (w <= 0) return false;
            p += w;
            n -= (size_t)w;
        }
        return true;
    }

    bool fill() {
        if (bpos < blen) return true;
        long n = rawRead(buf, BS);
        if (n <= 0) return false;
        bpos = 0;
        blen = (size_t)n;
        return true;
    }

    bool readLine(std::string& line) {
        line.clear();
        for (;;) {
            if (!fill()) return !line.empty();
            while (bpos < blen) {
                char c = (char)buf[bpos++];
                if (c == '\n') {
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    return true;
                }
                line += c;
                if (line.size() > 16384) return false;
            }
        }
    }

    bool readExact(uint64_t n, Sink& sink) {
        while (n > 0) {
            if (!fill()) return false;
            size_t take = (size_t)std::min<uint64_t>(n, blen - bpos);
            if (!sink.put(buf + bpos, take)) return false;
            bpos += take;
            n -= take;
        }
        return true;
    }

    bool readToEof(Sink& sink) {
        for (;;) {
            if (!fill()) return true;
            if (!sink.put(buf + bpos, blen - bpos)) return false;
            bpos = blen;
        }
    }
};

struct BodySrc {
    const uint8_t* data = nullptr;
    size_t len = 0;
};

// host/porta/TLS de destino
struct Target {
    std::string host;
    int port = 443;
    bool tls = true;
};

static Target apiTarget(const std::string& host) {
    Target t;
    t.host = host;
#ifdef HOST_TEST
    t.host = "127.0.0.1";
    t.port = testPort;
    t.tls = false;
#endif
    return t;
}

static bool httpRequest(const Target& tg, const std::string& hostHeader, const std::string& method, const std::string& path,
                        const std::vector<std::string>& headers, const BodySrc* body, Progress* wp, Resp& resp, Sink& sink) {
    Conn c;
    if (!c.connectTo(tg.host, tg.port, tg.tls)) return false;

    std::string head = method + " " + path + " HTTP/1.1\r\nHost: " + hostHeader + "\r\nUser-Agent: SwitchManager/0.2\r\nConnection: close\r\nAccept-Encoding: identity\r\n";
    for (auto& h : headers) head += h + "\r\n";
    if (body) head += "Content-Length: " + std::to_string(body->len) + "\r\n";
    else if (method == "POST" || method == "PUT" || method == "PATCH") head += "Content-Length: 0\r\n";
    head += "\r\n";
    if (!c.rawWriteAll(head.data(), head.size())) { setErr("Falha ao enviar o pedido."); return false; }
    if (body && body->len) {
        size_t off = 0;
        while (off < body->len) {
            size_t n = std::min<size_t>(body->len - off, 131072);
            if (!c.rawWriteAll(body->data + off, n)) { setErr("Falha ao enviar os dados."); return false; }
            off += n;
            if (wp && !wp->tick(n)) { setErr("Cancelado."); return false; }
        }
    }

    std::string line;
    do {
        if (!c.readLine(line)) { setErr("Sem resposta do servidor."); return false; }
        if (line.compare(0, 5, "HTTP/") != 0) { setErr("Resposta inválida do servidor."); return false; }
        size_t sp = line.find(' ');
        resp.status = sp == std::string::npos ? 0 : atoi(line.c_str() + sp + 1);
        resp.h.clear();
        for (;;) {
            if (!c.readLine(line)) { setErr("Resposta incompleta."); return false; }
            if (line.empty()) break;
            size_t col = line.find(':');
            if (col == std::string::npos) continue;
            resp.h[lowerStr(line.substr(0, col))] = trim(line.substr(col + 1));
        }
    } while (resp.status == 100);

    sink.onHeaders(resp);
    if (method == "HEAD" || resp.status == 204 || resp.status == 304) return true;

    auto te = resp.h.find("transfer-encoding");
    if (te != resp.h.end() && lowerStr(te->second).find("chunked") != std::string::npos) {
        for (;;) {
            if (!c.readLine(line)) { setErr("Resposta interrompida."); return false; }
            uint64_t sz = strtoull(line.c_str(), nullptr, 16);
            if (sz == 0) {
                while (c.readLine(line) && !line.empty()) {}
                break;
            }
            if (!c.readExact(sz, sink)) { setErr("Download interrompido."); return false; }
            if (!c.readLine(line)) { setErr("Resposta interrompida."); return false; }
        }
        return true;
    }
    auto cl = resp.h.find("content-length");
    if (cl != resp.h.end()) {
        uint64_t n = strtoull(cl->second.c_str(), nullptr, 10);
        if (!c.readExact(n, sink)) { setErr("Download interrompido."); return false; }
        return true;
    }
    return c.readToEof(sink);
}

// ----------------------------------------------------------------- OAuth

static const char* AUTH_HOST = "accounts.google.com";
static const char* TOKEN_HOST = "oauth2.googleapis.com";
static const char* API_HOST = "www.googleapis.com";

struct Creds {
    std::string id, secret, redirect;
    bool ok = false;
};
static Creds g_creds;
static std::string g_access, g_refresh;
static time_t g_expiry = 0;
static bool g_loginConfirmed = false;
struct Account {
    std::string key, client, refresh, legacyToken, name, permission;
    std::set<std::string> purgeHashes;
    bool keep = true, enabled = false, forgotten = false;
};
static Account g_account;
static bool g_accountLoaded = false;
static std::string g_durableAccount;
struct PendingLogin {
    std::string access;
    Account account;
    time_t expiry = 0;
    bool valid = false;
};
static PendingLogin g_pendingLogin;
static bool g_changing = false;
struct PreviousSession {
    Account account;
    std::string access, refresh;
    time_t expiry = 0;
    bool confirmed = false;
};
static PreviousSession g_previous;
static std::string g_oauthState, g_codeVerifier;

static bool loadCreds() {
    if (g_creds.ok) return true;
    std::string txt;
    if (!readFile(persist::File::Credentials, txt)) return false;
    JV j;
    const JV* in = nullptr;
    if (jparse(txt, j)) { in = j.get("installed"); if (!in) in = j.get("web"); }
    if (!in || in->t != JV::Obj) {
        persist::Error invalid; invalid.stage = "validar-credencial"; invalid.code = EINVAL;
        storageError("Credencial local inválida; arquivo preservado", invalid); return false;
    }
    const JV* id = in->get("client_id");
    const JV* secret = in->get("client_secret");
    g_creds.id = id && id->t == JV::Str ? id->s : "";
    g_creds.secret = secret && secret->t == JV::Str ? secret->s : "";
    g_creds.redirect = "http://localhost"; // compatibilidade com a credencial individual existente
    g_creds.ok = !g_creds.id.empty() && !g_creds.secret.empty();
    if (!g_creds.ok) {
        persist::Error invalid; invalid.stage = "validar-credencial"; invalid.code = EINVAL;
        storageError("Credencial local incompleta; arquivo preservado", invalid);
    }
    return g_creds.ok;
}

static std::string accountJson(const Account& value) {
    std::string hashes = "[";
    for (const auto& hash : value.purgeHashes) { if (hashes.size() > 1) hashes += ","; hashes += "\"" + jescape(hash) + "\""; }
    hashes += "]";
    return "{\"schema\":1,\"key\":\"" + jescape(value.key) + "\",\"client\":\"" + jescape(value.client) +
        "\",\"refresh\":\"" + jescape(value.refresh) + "\",\"legacy\":\"" + jescape(value.legacyToken) +
        "\",\"name\":\"" + jescape(value.name) + "\",\"permission\":\"" + jescape(value.permission) +
        "\",\"keep\":" + (value.keep ? "true" : "false") + ",\"enabled\":" + (value.enabled ? "true" : "false") +
        ",\"forgotten\":" + (value.forgotten ? "true" : "false") + ",\"cleanup\":" + hashes + "}";
}
static bool parseAccount(const std::string& text, Account& value) {
    JV j;
    if (!jparse(text, j) || j.t != JV::Obj || !j.get("schema") || j.get("schema")->t != JV::Num || j.get("schema")->n != 1) return false;
    for (auto field : {"key", "client", "refresh", "legacy", "name", "permission"}) {
        const JV* v = j.get(field); if (!v || v->t != JV::Str) return false;
    }
    for (auto field : {"keep", "enabled", "forgotten"}) {
        const JV* v = j.get(field); if (!v || v->t != JV::Bool) return false;
    }
    value.key = j.str("key"); value.client = j.str("client"); value.refresh = j.str("refresh");
    value.legacyToken = j.str("legacy"); value.name = j.str("name"); value.permission = j.str("permission");
    value.purgeHashes.clear();
    if (const JV* hashes = j.get("cleanup")) {
        if (hashes->t != JV::Arr) return false;
        for (const JV& hash : hashes->a) {
            if (hash.t != JV::Str || hash.s.size() != 43) return false;
            value.purgeHashes.insert(hash.s);
        }
    }
    value.keep = j.get("keep")->b; value.enabled = j.get("enabled")->b; value.forgotten = j.get("forgotten")->b;
    return !value.key.empty() && !(value.forgotten && (!value.refresh.empty() || !value.legacyToken.empty() || value.enabled));
}
static bool accountError(const char* stage, const char* message) {
    persist::Error error; error.stage = stage; error.code = EINVAL;
    storageError(message, error); return false;
}
static bool loadAccount() {
    if (g_accountLoaded) return true;
    std::string text;
    persist::Error error;
    if (persist::load(configDir(), persist::File::Account, text, error)) {
        if (!parseAccount(text, g_account)) return accountError("validar-conta", "Estado da conta inválido; arquivo preservado");
        g_durableAccount = text; g_accountLoaded = true; return true;
    }
    if (error.code != ENOENT) { storageError("Falha ao ler estado da conta", error); return false; }
    Account legacy;
    if (!sfm::oauth::randomToken(legacy.key)) { setErr("Não foi possível gerar identificador seguro da sessão."); return false; }
    legacy.client = g_creds.id;
    std::string disabled;
    if (persist::load(configDir(), persist::File::LoginState, disabled, error)) {
        if (disabled != "0" && disabled != "1") return accountError("validar-estado-login", "Estado de conexão inválido");
    } else if (error.code != ENOENT) { storageError("Falha ao ler desconexão legada", error); return false; }
    if (persist::load(configDir(), persist::File::Token, text, error)) {
        legacy.refresh = trim(text);
        if (legacy.refresh.empty()) return accountError("validar-token-salvo", "Login salvo vazio; arquivo preservado");
        legacy.legacyToken = legacy.refresh;
    } else if (error.code != ENOENT) { storageError("Falha ao ler token legado", error); return false; }
    legacy.enabled = !legacy.refresh.empty() && disabled != "1";
    g_account = std::move(legacy); g_accountLoaded = true;
    // O primeiro registro novo pode restaurar este estado sem alterar os arquivos legados.
    g_durableAccount = accountJson(g_account);
    return true;
}
static bool storeAccount(const Account& value) {
    std::string text = accountJson(value);
    persist::Error error;
    if (!persist::save(configDir(), persist::File::Account, text, error)) {
        storageError("Falha ao salvar login no cartão SD.", error); return false;
    }
    std::string confirmed;
    if (!persist::load(configDir(), persist::File::Account, confirmed, error, "confirmacao") || confirmed != text) {
        if (!error.code) { error.stage = "comparar-conta-confirmacao"; error.code = EIO; }
        persist::Error recovery;
        if (!g_durableAccount.empty() && !persist::save(configDir(), persist::File::Account, g_durableAccount, recovery)) {
            error.recoveryStage = recovery.stage; error.recoveryCode = recovery.code;
        }
        storageError("Conta não confirmada; restauração do estado anterior solicitada", error);
        return false;
    }
    g_durableAccount = text;
    return true;
}

bool hasCredentials() { return loadCreds(); }
bool isLoggedIn() { return !g_changing && g_loginConfirmed && !g_access.empty() && !g_refresh.empty(); }
ConnectionState connectionState() { return g_connectionState; }
std::string connectionLabel() {
    switch (g_connectionState) {
        case ConnectionState::Connected: return "Conectado";
        case ConnectionState::Disconnected: return "Desconectado";
        case ConnectionState::AuthorizationRequired: return "Autorização necessária";
        case ConnectionState::InvalidAuthorization: return "Autorização expirada ou inválida";
        case ConnectionState::NetworkError: return "Erro de rede";
        case ConnectionState::StorageError: return "Erro de armazenamento no SD";
    }
    return "Autorização necessária";
}
std::string accountName() { return g_account.name; }
bool keepConnected() { return loadAccount() && g_account.keep; }
bool setKeepConnected(bool keep) {
    clearError();
    if (!loadAccount()) return false;
    Account next = g_account; next.keep = keep;
    if (!storeAccount(next)) return false;
    g_account = std::move(next); return true;
}
bool autoLoginAvailable() {
    if (!loadCreds() || !loadAccount()) return false;
    bool available = g_account.keep && g_account.enabled && !g_account.forgotten && !g_account.refresh.empty();
    if (!available && !isLoggedIn()) g_connectionState = !g_account.refresh.empty() && !g_account.forgotten ? ConnectionState::Disconnected : ConnectionState::AuthorizationRequired;
    return available;
}

static void clearSession() {
    g_access.clear(); g_refresh.clear(); g_expiry = 0;
    g_loginConfirmed = false; g_pendingLogin = {};
}
bool logout() {
    clearError();
    clearSession(); g_changing = false; g_previous = {};
    g_connectionState = ConnectionState::Disconnected;
    if (!loadAccount()) return false;
    Account next = g_account; next.enabled = false;
    if (!storeAccount(next)) return false;
    g_account = std::move(next);
    return true;
}

bool forgetAccount() {
    clearError();
    clearSession(); g_changing = false; g_previous = {};
    g_connectionState = ConnectionState::Disconnected;
    if (!loadAccount()) return false;
    const Account target = g_account;
    std::set<std::string> tokens;
    if (!target.refresh.empty()) tokens.insert(target.refresh);
    if (!target.legacyToken.empty()) tokens.insert(target.legacyToken);
    Account forgotten = target;
    forgotten.refresh.clear(); forgotten.legacyToken.clear(); forgotten.enabled = false;
    forgotten.forgotten = true; forgotten.name.clear();
    // Primeiro bloquear restauração, inclusive do legado, antes de apagar qualquer cópia.
    if (!storeAccount(forgotten)) return false;
    g_account = forgotten;
    unsigned unknown = 0;
    persist::Error error, first;
    auto belongs = [&](const Account& value) {
        return value.key == target.key || (!target.permission.empty() && value.permission == target.permission && value.client == target.client);
    };
    std::set<std::string> otherHashes;
    auto collect = [&](const std::string& text) {
        Account value;
        if (parseAccount(text, value) && belongs(value)) {
            if (!value.refresh.empty()) tokens.insert(value.refresh);
            if (!value.legacyToken.empty()) tokens.insert(value.legacyToken);
            forgotten.purgeHashes.insert(value.purgeHashes.begin(), value.purgeHashes.end());
        } else if (parseAccount(text, value)) {
            if (!value.refresh.empty()) otherHashes.insert(sfm::oauth::challenge(value.refresh));
            if (!value.legacyToken.empty()) otherHashes.insert(sfm::oauth::challenge(value.legacyToken));
        }
        return persist::EraseDecision::Preserve;
    };
    if (!persist::eraseMatching(configDir(), persist::File::Account, collect, unknown, error)) {
        storageError("Acesso bloqueado; não foi possível identificar todas as cópias", error); return false;
    }
    for (const auto& token : tokens) forgotten.purgeHashes.insert(sfm::oauth::challenge(token));
    // Impressões SHA-256 permitem retomar uma exclusão parcial sem persistir novamente os tokens.
    if (!storeAccount(forgotten)) return false;
    g_account = forgotten;
    auto accountDecision = [&](const std::string& text) {
        Account value;
        if (!parseAccount(text, value)) return persist::EraseDecision::Unknown;
        bool match = belongs(value);
        if (!match || (value.refresh.empty() && value.legacyToken.empty())) return persist::EraseDecision::Preserve;
        if (!value.refresh.empty()) tokens.insert(value.refresh);
        if (!value.legacyToken.empty()) tokens.insert(value.legacyToken);
        return persist::EraseDecision::Remove;
    };
    bool ok = persist::eraseMatching(configDir(), persist::File::Account, accountDecision, unknown, error);
    if (!ok) first = error;
    auto tokenDecision = [&](const std::string& text) {
        std::string hash = sfm::oauth::challenge(trim(text));
        if (forgotten.purgeHashes.count(hash)) return persist::EraseDecision::Remove;
        return otherHashes.count(hash) ? persist::EraseDecision::Preserve : persist::EraseDecision::Unknown;
    };
    if (!persist::eraseMatching(configDir(), persist::File::Token, tokenDecision, unknown, error)) {
        if (ok) first = error;
        ok = false;
    }
    if (!ok) { storageError("Acesso bloqueado, mas restaram dados locais de autenticação. Tente Esquecer novamente", first); return false; }
    if (unknown) return accountError("identificar-copias", "Acesso bloqueado; cópias não atribuíveis à conta foram preservadas");
    clearError(); g_connectionState = ConnectionState::Disconnected;
    sfm_diag::write("DRIVE_AUTH", "etapa=esquecimento-local-concluido revogacao_remota=nao-executada");
    return true;
}

static bool persistPendingLogin() {
    if (!g_pendingLogin.valid) { setErr("Não há autorização pendente para salvar."); return false; }
    if (!ensureConfigDir() || !storeAccount(g_pendingLogin.account)) return false;
    g_account = g_pendingLogin.account;
    g_access = g_pendingLogin.access; g_refresh = g_account.refresh; g_expiry = g_pendingLogin.expiry;
    g_loginConfirmed = true; g_pendingLogin = {}; g_changing = false; g_previous = {};
    g_connectionState = ConnectionState::Connected;
    clearError();
    sfm_diag::write("DRIVE_AUTH", "etapa=login-gravado-relido-confirmado errno=0");
    return true;
}

static std::string apiErrorText(const std::string& body, int status) {
    JV j;
    if (jparse(body, j)) {
        const JV* e = j.get("error");
        if (e && e->t == JV::Obj) {
            std::string m = e->str("message");
            if (!m.empty()) return m;
        }
        if (e && e->t == JV::Str) {
            std::string d = j.str("error_description");
            return d.empty() ? e->s : d;
        }
    }
    return "Erro do servidor (HTTP " + std::to_string(status) + ").";
}

static bool tokenRequest(const std::string& form, bool renewing) {
    clearError();
    if (!g_changing) g_loginConfirmed = false;
    Target tg = apiTarget(TOKEN_HOST);
    std::vector<std::string> h = {"Content-Type: application/x-www-form-urlencoded"};
    BodySrc b;
    b.data = (const uint8_t*)form.data(); b.len = form.size();
    Resp r; StrSink sk;
    if (!httpRequest(tg, TOKEN_HOST, "POST", "/token", h, &b, nullptr, r, sk)) {
        setErr("Rede: " + g_err, ErrorSource::Network);
        sfm_diag::write("DRIVE_AUTH", "etapa=rede-token resultado=falha");
        return false;
    }
    JV j;
    if (r.status != 200) {
        // Nunca devolver corpo/descrição que possa ecoar segredo ou código de autorização.
        std::string code;
        if (jparse(sk.s, j)) code = j.str("error");
        static const char* known[] = {"invalid_grant", "invalid_client", "access_denied", "invalid_request", "unauthorized_client", "temporarily_unavailable", "server_error"};
        std::string safeCode = "erro_oauth";
        for (auto allowed : known) if (code == allowed) safeCode = allowed;
        setErr("Google: autorização não concluída (HTTP " + std::to_string(r.status) + "; " + safeCode + "). Dados salvos preservados.", ErrorSource::Google);
        if (safeCode == "invalid_grant") g_connectionState = ConnectionState::InvalidAuthorization;
        sfm_diag::write("DRIVE_AUTH", "etapa=resposta-google http=" + std::to_string(r.status));
        return false;
    }
    if (!jparse(sk.s, j)) { setErr("Google: resposta de autorização inválida.", ErrorSource::Protocol); return false; }
    PendingLogin candidate;
    const JV* access = j.get("access_token");
    const JV* refresh = j.get("refresh_token");
    if (!access || access->t != JV::Str || (refresh && refresh->t != JV::Str)) {
        setErr("Google: resposta de autorização com campos inválidos.", ErrorSource::Protocol); return false;
    }
    candidate.access = access->s;
    candidate.account = renewing ? g_account : Account{};
    candidate.account.keep = g_account.keep;
    candidate.account.client = g_creds.id;
    candidate.account.enabled = true;
    candidate.account.forgotten = false;
    if (candidate.account.key.empty() && !sfm::oauth::randomToken(candidate.account.key)) {
        setErr("Não foi possível gerar identificador seguro da conta."); return false;
    }
    candidate.account.refresh = refresh ? refresh->s : std::string();
    if (candidate.account.refresh.empty() && renewing) candidate.account.refresh = g_refresh;
    if (candidate.access.empty() || candidate.account.refresh.empty()) {
        setErr("Google: resposta sem os dados necessários para manter o login. Arquivos anteriores preservados.", ErrorSource::Protocol); return false;
    }
    long seconds = atol(j.str("expires_in").c_str());
    candidate.expiry = time(nullptr) + (seconds > 0 ? seconds : 3000);
    if (!renewing || candidate.account.permission.empty()) {
        Resp identityResponse; StrSink identity;
        std::vector<std::string> identityHeaders = {"Authorization: Bearer " + candidate.access};
        if (httpRequest(apiTarget(API_HOST), API_HOST, "GET", "/drive/v3/about?fields=user(displayName,permissionId)", identityHeaders, nullptr, nullptr, identityResponse, identity) && identityResponse.status == 200) {
            JV profile;
            if (jparse(identity.s, profile)) {
                const JV* user = profile.get("user");
                if (user && user->t == JV::Obj) {
                    candidate.account.permission = user->str("permissionId");
                    std::string name = user->str("displayName");
                    candidate.account.name.clear();
                    for (unsigned char c : name) if (c >= 32 && c != 127) candidate.account.name += char(c);
                    if (candidate.account.name.size() > 240) candidate.account.name.clear();
                }
            }
        }
        clearError(); // identificação é opcional; não altera resultado OAuth/persistência
    }
    candidate.valid = true;
    g_pendingLogin = std::move(candidate);
    return persistPendingLogin();
}

static bool refreshToken() {
    if (g_pendingLogin.valid) return persistPendingLogin();
    if (g_refresh.empty()) { setErr("Você não está conectado ao Google Drive."); return false; }
    if (!loadCreds()) {
        if (g_errorSource != ErrorSource::Storage) setErr("Credencial Google não configurada.");
        return false;
    }
    std::string form = "client_id=" + urlEncode(g_creds.id) + "&client_secret=" + urlEncode(g_creds.secret) +
                       "&refresh_token=" + urlEncode(g_refresh) + "&grant_type=refresh_token";
    // Inclusive invalid_grant deixa o arquivo original disponível para diagnóstico/recuperação.
    return tokenRequest(form, true);
}

static bool restoreLogin(bool explicitRequest) {
    clearError();
    if (g_pendingLogin.valid) return persistPendingLogin();
    if (g_changing) { setErr("Finalize ou cancele a troca de conta."); return false; }
    g_loginConfirmed = false;
    if (!loadCreds() || !loadAccount()) return false;
    if (g_account.forgotten || g_account.refresh.empty()) {
        g_connectionState = ConnectionState::AuthorizationRequired;
        setErr("Autorização necessária. Use Entrar com Google."); return false;
    }
    if (!explicitRequest && (!g_account.keep || !g_account.enabled)) {
        g_connectionState = ConnectionState::Disconnected;
        setErr("Conta desconectada ou Manter conectado desativado. Use Reconectar conta salva."); return false;
    }
    if (!g_account.client.empty() && g_account.client != g_creds.id) {
        g_connectionState = ConnectionState::AuthorizationRequired;
        setErr("A credencial do aplicativo mudou. Autorize novamente; dados anteriores preservados."); return false;
    }
    g_refresh = g_account.refresh;
    return refreshToken();
}
bool loadSavedLogin() { return restoreLogin(false); }
bool reconnectSavedLogin() { return restoreLogin(true); }

static bool ensureToken() {
    if (g_changing) { setErr("Operações remotas pausadas durante a troca de conta."); return false; }
    if (!g_loginConfirmed) { setErr("Entre ou reconecte a conta antes de acessar o Drive."); return false; }
    if (!g_access.empty() && time(nullptr) < g_expiry - 90) return true;
    return refreshToken();
}

static bool exchangeCode(const std::string& code) {
    if (!loadCreds()) {
        if (g_errorSource != ErrorSource::Storage) setErr("Credencial Google não configurada.");
        return false;
    }
    std::string f = "code=" + urlEncode(code) + "&client_id=" + urlEncode(g_creds.id) + "&client_secret=" + urlEncode(g_creds.secret) +
                    "&redirect_uri=" + urlEncode(g_creds.redirect) + "&code_verifier=" + urlEncode(g_codeVerifier) + "&grant_type=authorization_code";
    if (!tokenRequest(f, false)) return false;
    if (g_refresh.empty()) { setErr("O Google não devolveu a permissão de acesso contínuo. Tente de novo."); return false; }
    return true;
}

// ----------------------------------------------------------------- chamadas à API

static bool apiCall(const std::string& method, const std::string& host, const std::string& path, const std::vector<std::string>& extra,
                    const BodySrc* body, Progress* wp, Resp& r, Sink& sink, bool allowRetry = true) {
    if (!ensureToken()) return false;
    std::vector<std::string> h = extra;
    h.push_back("Authorization: Bearer " + g_access);
    if (!httpRequest(apiTarget(host), host, method, path, h, body, wp, r, sink)) {
        if (g_err != "Cancelado.") setErr("Rede: " + g_err, ErrorSource::Network);
        return false;
    }
    if (r.status == 401 && allowRetry) {
        g_expiry = 0;
        return apiCall(method, host, path, extra, body, wp, r, sink, false);
    }
    return true;
}

static bool apiJson(const std::string& method, const std::string& path, const std::string& jsonBody, JV& out) {
    std::vector<std::string> h;
    BodySrc b;
    BodySrc* bp = nullptr;
    if (!jsonBody.empty()) {
        h.push_back("Content-Type: application/json; charset=UTF-8");
        b.data = (const uint8_t*)jsonBody.data();
        b.len = jsonBody.size();
        bp = &b;
    }
    Resp r;
    StrSink sk;
    if (!apiCall(method, API_HOST, path, h, bp, nullptr, r, sk)) return false;
    if (r.status < 200 || r.status >= 300) { setErr(apiErrorText(sk.s, r.status)); return false; }
    out = JV();
    if (!sk.s.empty() && !jparse(sk.s, out)) { setErr("Resposta inválida do Drive."); return false; }
    return true;
}

bool listFolder(const std::string& folderId, std::vector<Item>& out) {
    out.clear();
    std::string pageToken;
    do {
        std::string q = "'" + folderId + "' in parents and trashed=false";
        std::string path = "/drive/v3/files?q=" + urlEncode(q) + "&fields=" + urlEncode("nextPageToken,files(id,name,mimeType,size)") +
                           "&pageSize=1000&supportsAllDrives=true&includeItemsFromAllDrives=true";
        if (!pageToken.empty()) path += "&pageToken=" + urlEncode(pageToken);
        JV j;
        if (!apiJson("GET", path, "", j)) return false;
        const JV* files = j.get("files");
        if (files && files->t == JV::Arr) {
            for (auto& f : files->a) {
                Item it;
                it.id = f.str("id");
                it.name = f.str("name");
                it.mime = f.str("mimeType");
                it.size = strtoull(f.str("size").c_str(), nullptr, 10);
                it.dir = it.mime == "application/vnd.google-apps.folder";
                it.gdoc = !it.dir && it.mime.compare(0, 28, "application/vnd.google-apps.") == 0;
                out.push_back(it);
            }
        }
        pageToken = j.str("nextPageToken");
    } while (!pageToken.empty());
    std::sort(out.begin(), out.end(), [](const Item& a, const Item& b) {
        if (a.dir != b.dir) return a.dir;
        return lowerStr(a.name) < lowerStr(b.name);
    });
    return true;
}

struct FileSink : Sink {
    FILE* f = nullptr;
    Progress* pr = nullptr;
    bool toFile = false;
    bool cancelled = false;
    uint64_t received = 0, limit = 0;
    std::string errBody;
    void onHeaders(const Resp& r) override { toFile = (r.status == 200 || r.status == 206); }
    bool put(const uint8_t* p, size_t n) override {
        if (!toFile) {
            if (errBody.size() < 65536) errBody.append((const char*)p, n);
            return true;
        }
        if (n > limit - received) { setErr("Resposta maior que o arquivo esperado."); return false; }
        if (fwrite(p, 1, n, f) != n) {
            setErr("Falha ao gravar no cartão SD (cheio, ou cartão FAT32 com limite de 4 GB por arquivo).");
            return false;
        }
        received += n;
        if (pr && !pr->tick(n)) { cancelled = true; setErr("Cancelado."); return false; }
        return true;
    }
};

bool downloadFile(const Item& it, const std::string& dstPath, Progress* pr) {
    int fd = open(dstPath.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) { setErr("Não foi possível criar o arquivo no cartão SD (destino existente ou indisponível)."); return false; }
    FILE* f = fdopen(fd, "wb");
    if (!f) { close(fd); remove(dstPath.c_str()); setErr("Falha ao abrir destino do download."); return false; }
    bool ok = true;
    const uint64_t CH = 16ull << 20;
    uint64_t pos = 0;
    while (ok && pos < it.size) {
        uint64_t end = std::min<uint64_t>(pos + CH, it.size) - 1;
        std::string path = "/drive/v3/files/" + urlEncode(it.id) + "?alt=media&supportsAllDrives=true";
        std::vector<std::string> h = {"Range: bytes=" + std::to_string(pos) + "-" + std::to_string(end)};
        bool done = false;
        for (int attempt = 0; attempt < 3 && !done; attempt++) {
            if (fseeko(f, (off_t)pos, SEEK_SET) != 0) { setErr("Falha ao posicionar destino do download."); ok = false; break; }
            FileSink fs;
            fs.limit = it.size - pos;
            fs.f = f;
            fs.pr = pr;
            Resp r;
            if (!apiCall("GET", API_HOST, path, h, nullptr, nullptr, r, fs)) {
                if (fs.cancelled) { ok = false; break; }
                continue;
            }
            if (r.status == 206) {
                const std::string expectedRange = "bytes " + std::to_string(pos) + "-" + std::to_string(end) + "/" + std::to_string(it.size);
                if (fs.received == end - pos + 1 && r.h["content-range"] == expectedRange) {
                    done = true; pos = end + 1;
                } else setErr("Download incompleto ou intervalo de bytes inválido.");
            }
            else if (r.status == 200 && pos == 0 && fs.received == it.size) { done = true; pos = it.size; }
            else if (r.status == 416) { setErr("O servidor recusou o intervalo do download."); ok = false; break; }
            else if (r.status >= 500 || r.status == 429) continue;
            else { setErr(apiErrorText(fs.errBody, r.status)); ok = false; break; }
        }
        if (ok && !done) { if (g_err.empty()) setErr("Falha ao baixar o arquivo."); ok = false; }
    }
    if (ok && (fflush(f) != 0 || fsync(fileno(f)) != 0)) { setErr("Falha ao finalizar a gravação do download."); ok = false; }
    if (fclose(f) != 0) { setErr("Falha ao fechar o download."); ok = false; }
    if (!ok && remove(dstPath.c_str()) != 0) setErr("Falha no download; arquivo parcial permaneceu no destino.");
    return ok;
}

bool createFolder(const std::string& parentId, const std::string& name, std::string& newId) {
    std::string body = "{\"name\":\"" + jescape(name) + "\",\"mimeType\":\"application/vnd.google-apps.folder\",\"parents\":[\"" + jescape(parentId) + "\"]}";
    JV j;
    if (!apiJson("POST", "/drive/v3/files?fields=id&supportsAllDrives=true", body, j)) return false;
    newId = j.str("id");
    if (newId.empty()) { setErr("Resposta inválida ao criar a pasta."); return false; }
    return true;
}

bool renameItem(const std::string& id, const std::string& newName) {
    JV j;
    return apiJson("PATCH", "/drive/v3/files/" + urlEncode(id) + "?fields=id&supportsAllDrives=true", "{\"name\":\"" + jescape(newName) + "\"}", j);
}

bool trashItem(const std::string& id) {
    JV j;
    return apiJson("PATCH", "/drive/v3/files/" + urlEncode(id) + "?fields=id&supportsAllDrives=true", "{\"trashed\":true}", j);
}

bool uploadFile(const std::string& srcPath, const std::string& parentId, const std::string& name, Progress* pr) {
    FILE* f = fopen(srcPath.c_str(), "rb");
    if (!f) { setErr("Não foi possível abrir: " + name); return false; }
    fseeko(f, 0, SEEK_END);
    uint64_t total = (uint64_t)ftello(f);
    fseeko(f, 0, SEEK_SET);

    std::string meta = "{\"name\":\"" + jescape(name) + "\",\"parents\":[\"" + jescape(parentId) + "\"]}";
    if (total == 0) {
        fclose(f);
        JV j;
        return apiJson("POST", "/drive/v3/files?fields=id&supportsAllDrives=true", meta, j);
    }

    // 1) abre a sessão de envio
    std::string startPath = "/upload/drive/v3/files?uploadType=resumable&fields=id&supportsAllDrives=true";
    std::vector<std::string> h = {"Content-Type: application/json; charset=UTF-8", "X-Upload-Content-Length: " + std::to_string(total)};
    BodySrc mb;
    mb.data = (const uint8_t*)meta.data();
    mb.len = meta.size();
    Resp r;
    StrSink sk;
    if (!apiCall("POST", API_HOST, startPath, h, &mb, nullptr, r, sk)) { fclose(f); return false; }
    if (r.status != 200) { fclose(f); setErr(apiErrorText(sk.s, r.status)); return false; }
    std::string loc = r.h["location"];
    size_t hs = loc.find("://");
    size_t ps = hs == std::string::npos ? std::string::npos : loc.find('/', hs + 3);
    if (loc.empty() || ps == std::string::npos) { fclose(f); setErr("O Drive não abriu a sessão de envio."); return false; }
    std::string sessionPath = loc.substr(ps);

    // 2) envia por partes (múltiplos de 256 KB)
    const size_t CH = 8u << 20;
    std::vector<uint8_t> chunk(CH);
    uint64_t pos = 0;
    bool ok = true;
    while (ok && pos < total) {
        size_t n = (size_t)std::min<uint64_t>(CH, total - pos);
        fseeko(f, (off_t)pos, SEEK_SET);
        if (fread(chunk.data(), 1, n, f) != n) { setErr("Falha ao ler o arquivo: " + name); ok = false; break; }
        std::vector<std::string> ch = {"Content-Range: bytes " + std::to_string(pos) + "-" + std::to_string(pos + n - 1) + "/" + std::to_string(total)};
        BodySrc cb;
        cb.data = chunk.data();
        cb.len = n;
        bool sent = false;
        for (int attempt = 0; attempt < 3 && !sent; attempt++) {
            Resp cr;
            StrSink csk;
            Progress* wp = attempt == 0 ? pr : nullptr;  // só reporta o progresso na primeira tentativa
            if (!apiCall("PUT", API_HOST, sessionPath, ch, &cb, wp, cr, csk)) {
                if (g_err == "Cancelado.") { ok = false; break; }
                continue;
            }
            if (cr.status == 308 || cr.status == 200 || cr.status == 201) sent = true;
            else if (cr.status >= 500 || cr.status == 429) continue;
            else { setErr(apiErrorText(csk.s, cr.status)); ok = false; break; }
        }
        if (ok && !sent) { if (g_err.empty()) setErr("Falha ao enviar o arquivo."); ok = false; }
        pos += n;
    }
    fclose(f);
    return ok;
}

// ----------------------------------------------------------------- servidor local de login

static int g_srv = -1;
static std::string g_ipText;

static std::string htmlHead() {
    return "<!doctype html><html lang=\"pt-BR\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
           "<title>Switch Manager</title><style>"
           "body{margin:0;background:#0e111a;color:#e8edf7;font-family:sans-serif;padding:18px}"
           "h1{font-size:22px;color:#60a5fa;margin:6px 0 14px}p{line-height:1.5;color:#c3cbe0}"
           ".c{background:#181d2b;border-radius:14px;padding:16px;margin:14px 0}"
           "a.b,button{display:block;width:100%;box-sizing:border-box;text-align:center;background:#3468c4;color:#fff;border:0;border-radius:12px;"
           "padding:14px;font-size:17px;text-decoration:none;margin-top:10px}"
           "textarea{width:100%;box-sizing:border-box;height:110px;background:#0e111a;color:#e8edf7;border:1px solid #3a4560;border-radius:10px;padding:10px;font-size:14px}"
           "b{color:#fff}</style></head><body><h1>Switch Manager</h1>";
}

static std::string authUrl() {
    return std::string("https://") + AUTH_HOST + "/o/oauth2/v2/auth?client_id=" + urlEncode(g_creds.id) +
           "&redirect_uri=" + urlEncode(g_creds.redirect) + "&response_type=code&access_type=offline&prompt=consent%20select_account&state=" + urlEncode(g_oauthState) +
           "&code_challenge_method=S256&code_challenge=" + sfm::oauth::challenge(g_codeVerifier) + "&scope=" +
           urlEncode("https://www.googleapis.com/auth/drive");
}

static std::string pageMain(const std::string& note) {
    std::string h = htmlHead();
    if (!note.empty()) {
        std::string escaped;
        for (char c : note) {
            if (c == '&') escaped += "&amp;";
            else if (c == '<') escaped += "&lt;";
            else if (c == '>') escaped += "&gt;";
            else escaped += c;
        }
        h += "<div class=\"c\"><b>" + escaped + "</b></div>";
    }
    if (g_pendingLogin.valid) {
        h += "<div class=\"c\"><p>A autorização foi recebida. O login ainda não foi confirmado no cartão SD.</p>"
             "<form method=\"post\" action=\"/retry-save\"><button type=\"submit\">Tentar salvar login novamente</button></form></div>";
    }
    if (!g_creds.ok) {
        h += "<div class=\"c\"><p><b>Passo 1 de 2:</b> envie o arquivo JSON da credencial do Google (o arquivo <b>client_secret...json</b>).</p>"
             "<form method=\"post\" action=\"/cred\">"
             "<input type=\"file\" id=\"f\" accept=\".json,application/json\" style=\"width:100%;margin:8px 0\">"
             "<textarea name=\"j\" id=\"j\" placeholder=\"O conteúdo do arquivo aparece aqui\"></textarea>"
             "<button type=\"submit\">Salvar credencial</button></form></div>"
             "<script>document.getElementById('f').addEventListener('change',function(e){var r=new FileReader();"
             "r.onload=function(){document.getElementById('j').value=r.result};r.readAsText(e.target.files[0]);});</script>";
    } else {
        h += "<div class=\"c\"><p><b>Passo 1:</b> toque abaixo, entre na sua conta Google e permita o acesso. "
             "Se aparecer \"app não verificado\", toque em <b>Avançado</b> e depois em <b>Acessar</b>.</p>"
             "<a class=\"b\" target=\"_blank\" href=\"" + authUrl() + "\">Entrar com o Google</a></div>"
             "<div class=\"c\"><p><b>Passo 2:</b> depois de permitir, o navegador vai mostrar uma página de erro (\"localhost\"). "
             "Isso é normal! Copie o <b>endereço completo</b> dessa página (na barra do navegador) e cole aqui:</p>"
             "<form method=\"post\" action=\"/code\"><textarea name=\"u\" placeholder=\"http://localhost/?code=...\"></textarea>"
             "<button type=\"submit\">Enviar para o Switch</button></form></div>";
    }
    size_t cursor = 0;
    while ((cursor = h.find("<form ", cursor)) != std::string::npos) {
        cursor = h.find('>', cursor);
        if (cursor == std::string::npos) break;
        std::string hidden = "<input type=\"hidden\" name=\"csrf\" value=\"" + g_oauthState + "\">";
        h.insert(++cursor, hidden); cursor += hidden.size();
    }
    h += "</body></html>";
    return h;
}

static std::string pageDone(const std::string& msg) {
    return htmlHead() + "<div class=\"c\"><p>" + msg + "</p></div></body></html>";
}

bool loginServerStart(std::string& urlForUser) {
    if (g_srv >= 0) { urlForUser = "http://" + g_ipText + ":" + std::to_string(
#ifdef HOST_TEST
        testLoginPort
#else
        8080
#endif
    ); return true; }
    clearError();
    if (!ensureConfigDir()) return false;
    loadCreds();
    if (!loadAccount()) return false;
    if (!sfm::oauth::randomToken(g_oauthState) || !sfm::oauth::randomToken(g_codeVerifier)) {
        setErr("Não foi possível preparar autorização segura."); return false;
    }
    g_srv = socket(AF_INET, SOCK_STREAM, 0);
    if (g_srv < 0) { setErr("Não foi possível abrir o servidor local."); return false; }
    int one = 1;
    setsockopt(g_srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
#ifdef HOST_TEST
    int port = testLoginPort;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
#else
    int port = 8080;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
#endif
    a.sin_port = htons((uint16_t)port);
    if (bind(g_srv, (sockaddr*)&a, sizeof a) != 0 || listen(g_srv, 4) != 0) {
        close(g_srv);
        g_srv = -1;
        setErr("A porta 8080 já está em uso.");
        return false;
    }
    int fl = fcntl(g_srv, F_GETFL, 0);
    fcntl(g_srv, F_SETFL, fl | O_NONBLOCK);

    in_addr ia;
#ifdef HOST_TEST
    ia.s_addr = htonl(INADDR_LOOPBACK);
#else
    ia.s_addr = (in_addr_t)gethostid();
#endif
    g_ipText = inet_ntoa(ia);
    urlForUser = "http://" + g_ipText + ":" + std::to_string(port);
    g_previous = {g_account, g_access, g_refresh, g_expiry, g_loginConfirmed};
    g_changing = true; g_pendingLogin = {};
    g_connectionState = ConnectionState::AuthorizationRequired;
    return true;
}

void loginServerStop() {
    if (g_srv >= 0) { close(g_srv); g_srv = -1; }
    if (g_changing) {
        g_account = g_previous.account; g_access = g_previous.access; g_refresh = g_previous.refresh;
        g_expiry = g_previous.expiry; g_loginConfirmed = g_previous.confirmed;
        if (g_loginConfirmed) { clearError(); g_connectionState = ConnectionState::Connected; }
        g_pendingLogin = {}; g_previous = {}; g_changing = false;
    }
    g_oauthState.clear(); g_codeVerifier.clear();
}

static std::string formField(const std::string& body, const char* key) {
    std::string k = std::string(key) + "=";
    size_t i = 0;
    while (i < body.size()) {
        size_t e = body.find('&', i);
        if (e == std::string::npos) e = body.size();
        if (body.compare(i, k.size(), k) == 0) return urlDecode(body.substr(i + k.size(), e - i - k.size()));
        i = e + 1;
    }
    return "";
}

static std::string extractCode(const std::string& in) {
    std::string s = trim(in);
    size_t i = s.find("code=");
    if (i == std::string::npos) {
        // aceita o código puro (começa com "4/")
        if (s.find(' ') == std::string::npos && s.size() > 10 && s.find("://") == std::string::npos) return urlDecode(s);
        return "";
    }
    i += 5;
    size_t e = s.find_first_of("& \r\n\"", i);
    std::string c = s.substr(i, e == std::string::npos ? std::string::npos : e - i);
    return urlDecode(c);
}

static void sendAll(int fd, const std::string& s) {
    const char* p = s.data();
    size_t n = s.size();
    while (n > 0) {
        long w = send(fd, p, n, 0);
        if (w <= 0) break;
        p += w;
        n -= (size_t)w;
    }
}

static void sendHtml(int fd, int status, const std::string& html) {
    std::string st = status == 200 ? "200 OK" : "400 Bad Request";
    std::string r = "HTTP/1.1 " + st + "\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " + std::to_string(html.size()) +
                    "\r\nConnection: close\r\n\r\n" + html;
    sendAll(fd, r);
}

int loginServerPoll() {
    if (g_srv < 0) return -1;
    sockaddr_in ca;
    socklen_t cl = sizeof ca;
    int c = accept(g_srv, (sockaddr*)&ca, &cl);
    if (c < 0) return 0;
    int fl = fcntl(c, F_GETFL, 0);
    fcntl(c, F_SETFL, fl & ~O_NONBLOCK);
    struct timeval tv;
    tv.tv_sec = 4;
    tv.tv_usec = 0;
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

    std::string req;
    char b[4096];
    size_t headEnd = std::string::npos;
    size_t need = 0;
    for (;;) {
        long n = recv(c, b, sizeof b, 0);
        if (n <= 0) break;
        req.append(b, (size_t)n);
        if (headEnd == std::string::npos) {
            headEnd = req.find("\r\n\r\n");
            if (headEnd != std::string::npos) {
                headEnd += 4;
                std::string hl = lowerStr(req.substr(0, headEnd));
                size_t p = hl.find("content-length:");
                need = p == std::string::npos ? 0 : (size_t)atol(hl.c_str() + p + 15);
            }
        }
        if (headEnd != std::string::npos && req.size() >= headEnd + need) break;
        if (req.size() > (1u << 20)) break;
    }
    if (headEnd == std::string::npos) { close(c); return 0; }

    std::string line = req.substr(0, req.find("\r\n"));
    std::string body = req.substr(headEnd);
    int result = 0;

    if (line.compare(0, 5, "POST ") == 0 && (g_oauthState.empty() || formField(body, "csrf") != g_oauthState)) {
        setErr("Pedido local não confirmado. Reabra a página de login.");
        sendHtml(c, 400, pageMain(g_err)); result = -1;
    } else if (line.compare(0, 11, "POST /cred ") == 0) {
        std::string json = formField(body, "j");
        JV j;
        const JV* in = nullptr;
        if (jparse(json, j)) { in = j.get("installed"); if (!in) in = j.get("web"); }
        if (g_previous.confirmed) {
            sendHtml(c, 400, pageMain("Desconecte antes de alterar a credencial do aplicativo; a troca de conta usa a credencial atual."));
        } else if (!in || in->t != JV::Obj || !in->get("client_id") || in->get("client_id")->t != JV::Str ||
                   !in->get("client_secret") || in->get("client_secret")->t != JV::Str ||
                   in->str("client_id").empty() || in->str("client_secret").empty()) {
            sendHtml(c, 400, pageMain("Esse arquivo não parece ser a credencial do Google. Escolha o arquivo client_secret...json."));
        } else {
            clearError();
            if (!writeFile(persist::File::Credentials, json)) {
                sendHtml(c, 400, pageMain(g_err));
                result = -1;
            } else {
                g_creds = Creds();
                g_pendingLogin = {};
                if (loadCreds()) sendHtml(c, 200, pageMain("Credencial salva! Agora faça o passo 1 e o passo 2."));
                else { sendHtml(c, 400, pageMain("SD: a credencial gravada não passou na releitura.")); result = -1; }
            }
        }
    } else if (line.compare(0, 17, "POST /retry-save ") == 0) {
        if (persistPendingLogin()) { sendHtml(c, 200, pageDone("Login gravado e conferido. Volte ao Switch.")); result = 1; }
        else { sendHtml(c, 400, pageMain(g_err)); result = -1; }
    } else if (line.compare(0, 10, "POST /code") == 0) {
        std::string code = extractCode(formField(body, "u"));
        std::string returned = trim(formField(body, "u"));
        size_t query = returned.find('?');
        bool validState = query != std::string::npos && formField(returned.substr(query + 1), "state") == g_oauthState;
        if (!validState) {
            setErr("O retorno não corresponde a esta autorização. Copie a URL completa com code e state.");
            sendHtml(c, 400, pageMain(g_err)); result = -1;
        } else if (g_pendingLogin.valid) {
            sendHtml(c, 400, pageMain("A autorização já foi recebida. Use Tentar salvar login novamente.")); result = -1;
        } else if (code.empty()) {
            sendHtml(c, 400, pageMain("Não encontrei o código nesse texto. Copie o endereço completo da página que deu erro."));
        } else if (exchangeCode(code)) {
            sendHtml(c, 200, pageDone("Pronto! Você já pode voltar ao Switch."));
            result = 1;
        } else {
            sendHtml(c, 400, pageMain(g_err));
            result = -1;
        }
    } else {
        sendHtml(c, 200, pageMain(""));
    }
    close(c);
    return result;
}

}  // namespace gd
