#include "updates/update_http.h"
#include "network/io_wait.h"
#include "network/socket_runtime.h"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <switch.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace sfm::updates {
#ifdef HOST_TEST
int httpTestPort = 0;
#endif
namespace {
std::string lower(std::string s) {
    for (char &c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
bool allowed(const std::string &url, std::string &host, std::string &path) {
    if (url.compare(0, 8, "https://") != 0 || url.find_first_of("\r\n\\") != std::string::npos)
        return false;
    size_t slash = url.find('/', 8);
    if (slash == std::string::npos)
        return false;
    host = url.substr(8, slash - 8);
    path = url.substr(slash);
    if (host == "api.github.com")
        return path.compare(0, strlen("/repos/TemudjinBR/switch-file-manager/releases"),
                            "/repos/TemudjinBR/switch-file-manager/releases") == 0;
    if (host == "github.com")
        return path.compare(0, strlen("/TemudjinBR/switch-file-manager/releases/download/"),
                            "/TemudjinBR/switch-file-manager/releases/download/") == 0;
    return host == "release-assets.githubusercontent.com";
}
struct Connection {
    int fd = -1, sslFd = -1;
    std::atomic<bool> &cancel;
    std::string &error;
#ifndef HOST_TEST
    SslContext context{};
    SslConnection ssl{};
    bool sslUp = false, contextUp = false, connectionUp = false;
#endif
    bool owner = false;
    Connection(std::atomic<bool> &c, std::string &e) : cancel(c), error(e) {}
    bool fail(const std::string &stage, int code = 0) {
        error = stage + (code ? " codigo=" + std::to_string(code) : "");
        return false;
    }
    bool wait(bool writing, unsigned milliseconds = 5000) {
        auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
        while (!cancel) {
            fd_set f;
            FD_ZERO(&f);
            FD_SET(fd, &f);
            timeval t{0, 0};
            int n = select(fd + 1, writing ? nullptr : &f, writing ? &f : nullptr, nullptr, &t);
            if (n > 0)
                return true;
            if (n < 0 && errno != EINTR)
                return fail("select", errno);
            if (std::chrono::steady_clock::now() >= end)
                return fail("Timeout TCP", ETIMEDOUT);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return fail("Cancelado");
    }
    bool open(const std::string &host) {
        if (cancel)
            return fail("Cancelado");
        if (!sfm::net::acquire(sfm::net::Client::Updates, error))
            return false;
        owner = true;
        addrinfo hints{}, *list = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
#ifdef HOST_TEST
        std::string service = std::to_string(httpTestPort);
        const char *name = "127.0.0.1";
#else
        std::string service = "443";
        const char *name = host.c_str();
#endif
        int dns = getaddrinfo(name, service.c_str(), &hints, &list);
        if (dns || !list)
            return fail("DNS", dns);
        fd = socket(list->ai_family, list->ai_socktype, list->ai_protocol);
        if (fd < 0) {
            freeaddrinfo(list);
            return fail("socket", errno);
        }
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
            freeaddrinfo(list);
            return fail("fcntl", errno);
        }
        int rc = connect(fd, list->ai_addr, list->ai_addrlen);
        freeaddrinfo(list);
        if (rc != 0) {
            if (errno != EINPROGRESS || !wait(true, 10000))
                return fail("Conectar", errno);
            int code = 0;
            socklen_t size = sizeof code;
            if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &code, &size) != 0 || code)
                return fail("Conectar", code ? code : errno);
        }
#ifndef HOST_TEST
        Result result = sslInitialize(3);
        if (R_FAILED(result))
            return fail("SSL iniciar", result);
        sslUp = true;
        result = sslCreateContext(&context, SslVersion_Auto);
        if (R_FAILED(result))
            return fail("SSL contexto", result);
        contextUp = true;
        result = sslContextCreateConnection(&context, &ssl);
        if (R_FAILED(result))
            return fail("SSL conexao", result);
        connectionUp = true;
        sslFd = socketSslConnectionSetSocketDescriptor(&ssl, fd);
        if (sslFd < 0 && errno != ENOENT)
            return fail("SSL socket", errno);
        result = sslConnectionSetHostName(&ssl, host.c_str(), host.size());
        if (R_FAILED(result))
            return fail("SSL nome", result);
        result = sslConnectionSetVerifyOption(&ssl, SslVerifyOption_PeerCa | SslVerifyOption_HostName |
                                                        SslVerifyOption_DateCheck);
        if (R_FAILED(result))
            return fail("SSL verificacao", result);
        result = sslConnectionSetIoMode(&ssl, SslIoMode_NonBlocking);
        if (R_FAILED(result))
            return fail("SSL modo", result);
        u32 bytes = 0, certs = 0;
        result = sfm::net::waitIo([&] { return sslConnectionDoHandshake(&ssl, &bytes, &certs, nullptr, 0); },
                                  [&] { return !cancel.load(); }, 15000);
        if (R_FAILED(result))
            return fail("SSL handshake/certificado", result);
#else
        (void)host;
#endif
        return true;
    }
    long read(void *buffer, size_t size) {
        if (cancel)
            return -1;
#ifdef HOST_TEST
        for (;;) {
            if (!wait(false))
                return -1;
            long n = recv(fd, buffer, size, 0);
            if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
                continue;
            return n;
        }
#else
        u32 bytes = 0;
        Result rc = sfm::net::waitIo([&] { return sslConnectionRead(&ssl, buffer, size, &bytes); },
                                     [&] { return !cancel.load(); }, 5000);
        if (R_FAILED(rc)) {
            fail("SSL ler", rc);
            return -1;
        }
        return bytes;
#endif
    }
    bool write(const std::string &text) {
        size_t offset = 0;
        while (offset < text.size() && !cancel) {
            long n;
#ifdef HOST_TEST
            if (!wait(true))
                return false;
            n = send(fd, text.data() + offset, text.size() - offset, 0);
            if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
                continue;
#else
            u32 size = 0;
            Result rc = sfm::net::waitIo(
                [&] { return sslConnectionWrite(&ssl, text.data() + offset, text.size() - offset, &size); },
                [&] { return !cancel.load(); }, 5000);
            if (R_FAILED(rc))
                return fail("SSL enviar", rc);
            n = size;
#endif
            if (n <= 0)
                return fail("Enviar HTTP", errno);
            offset += size_t(n);
        }
        return offset == text.size();
    }
    ~Connection() {
#ifndef HOST_TEST
        if (connectionUp)
            sslConnectionClose(&ssl);
        if (sslFd >= 0)
            close(sslFd);
        if (contextUp)
            sslContextClose(&context);
        if (sslUp)
            sslExit();
#endif
        if (fd >= 0)
            close(fd);
        if (owner)
            sfm::net::release(sfm::net::Client::Updates);
    }
};
} // namespace
bool getHttps(const std::string &initial, uint64_t limit, const Sink &sink, std::atomic<bool> &cancel,
              Response &response, std::string &error) {
    std::string url = initial;
    error.clear();
    for (unsigned redirects = 0; redirects < 4; ++redirects) {
        std::string host, path;
        if (!allowed(url, host, path)) {
            error = "URL/redirect fora das origens oficiais";
            return false;
        }
        Connection c(cancel, error);
        if (!c.open(host))
            return false;
        if (!c.write("GET " + path + " HTTP/1.1\r\nHost: " + host +
                     "\r\nUser-Agent: SwitchFileManager-Updater/1\r\nAccept: "
                     "application/vnd.github+json\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n"))
            return false;
        std::string head, pending;
        char buffer[32768];
        size_t end;
        while ((end = head.find("\r\n\r\n")) == std::string::npos) {
            long n = c.read(buffer, sizeof buffer);
            if (n <= 0) {
                if (error.empty())
                    error = "Resposta HTTP interrompida";
                return false;
            }
            head.append(buffer, n);
            if (head.size() > 65536) {
                error = "Cabecalho HTTP excessivo";
                return false;
            }
        }
        pending = head.substr(end + 4);
        head.resize(end + 2);
        response = {};
        if (sscanf(head.c_str(), "HTTP/1.%*d %d", &response.status) != 1) {
            error = "Status HTTP invalido";
            return false;
        }
        size_t p = head.find("\r\n") + 2;
        while (p < head.size()) {
            size_t next = head.find("\r\n", p);
            if (next == std::string::npos)
                break;
            std::string line = head.substr(p, next - p);
            p = next + 2;
            size_t colon = line.find(':');
            if (colon == std::string::npos) {
                error = "Cabecalho HTTP invalido";
                return false;
            }
            std::string key = lower(line.substr(0, colon)), value = line.substr(colon + 1);
            while (!value.empty() && value.front() == ' ')
                value.erase(0, 1);
            if (response.headers.count(key)) {
                error = "Cabecalho HTTP duplicado";
                return false;
            }
            response.headers[key] = value;
        }
        if (response.status == 301 || response.status == 302 || response.status == 303 ||
            response.status == 307 || response.status == 308) {
            url = response.headers["location"];
            continue;
        }
        if (response.status != 200) {
            error = "HTTP " + std::to_string(response.status) +
                    (response.status == 403 || response.status == 429 ? "; aguarde o limite da API" : "");
            return false;
        }
        // Transfer-Encoding chunked e aceito, com tamanho total e linhas limitados.
        bool chunked = lower(response.headers["transfer-encoding"]) == "chunked";
        uint64_t expected = UINT64_MAX;
        auto len = response.headers.find("content-length");
        if (len != response.headers.end()) {
            expected = 0;
            if (len->second.empty())
                return false;
            for (char digit : len->second) {
                if (digit < '0' || digit > '9' || expected > (UINT64_MAX - 9) / 10) {
                    error = "Content-Length invalido";
                    return false;
                }
                expected = expected * 10 + digit - '0';
            }
            if (expected > limit) {
                error = "Download excede limite";
                return false;
            }
        }
        if (!response.headers["content-encoding"].empty() &&
            response.headers["content-encoding"] != "identity") {
            error = "Codificacao HTTP nao suportada";
            return false;
        }
        if (!response.headers["transfer-encoding"].empty() && !chunked) {
            error = "Transfer-Encoding invalido";
            return false;
        }
        if (chunked && len != response.headers.end()) {
            error = "Tamanho HTTP ambiguo";
            return false;
        }
        auto more = [&]() {
            long n = c.read(buffer, sizeof buffer);
            if (n <= 0)
                return false;
            pending.append(buffer, n);
            return true;
        };
        auto emit = [&](const uint8_t *data, size_t size) {
            if (size > limit - response.bytes) {
                error = "Download excede limite";
                return false;
            }
            if (!sink(data, size)) {
                if (error.empty())
                    error = "Gravacao/cancelamento interrompeu download";
                return false;
            }
            response.bytes += size;
            return true;
        };
        if (chunked) {
            for (;;) {
                size_t newline;
                while ((newline = pending.find("\r\n")) == std::string::npos) {
                    if (pending.size() > 1024 || !more()) {
                        error = "Chunk incompleto";
                        return false;
                    }
                }
                std::string line = pending.substr(0, newline);
                pending.erase(0, newline + 2);
                uint64_t size = 0;
                if (line.empty() || line.size() > 16) {
                    error = "Chunk invalido";
                    return false;
                }
                for (char digit : line) {
                    int n = digit >= '0' && digit <= '9'   ? digit - '0'
                            : digit >= 'a' && digit <= 'f' ? digit - 'a' + 10
                            : digit >= 'A' && digit <= 'F' ? digit - 'A' + 10
                                                           : -1;
                    if (n < 0 || size > (UINT64_MAX - 15) / 16) {
                        error = "Chunk invalido";
                        return false;
                    }
                    size = size * 16 + n;
                }
                if (!size) {
                    while (pending.size() < 2)
                        if (!more())
                            return false;
                    if (pending.substr(0, 2) != "\r\n") {
                        error = "Trailers HTTP nao suportados";
                        return false;
                    }
                    return true;
                }
                if (size > limit - response.bytes) {
                    error = "Chunk excede limite";
                    return false;
                }
                while (size) {
                    if (pending.empty() && !more()) {
                        error = "Download parcial";
                        return false;
                    }
                    size_t n = size_t(std::min<uint64_t>(size, pending.size()));
                    if (!emit(reinterpret_cast<const uint8_t *>(pending.data()), n))
                        return false;
                    pending.erase(0, n);
                    size -= n;
                }
                while (pending.size() < 2)
                    if (!more())
                        return false;
                if (pending.substr(0, 2) != "\r\n") {
                    error = "Terminador chunk invalido";
                    return false;
                }
                pending.erase(0, 2);
            }
        }
        do {
            if (!pending.empty()) {
                if (expected != UINT64_MAX && pending.size() > expected - response.bytes) {
                    error = "Corpo maior que Content-Length";
                    return false;
                }
                if (!emit(reinterpret_cast<const uint8_t *>(pending.data()), pending.size()))
                    return false;
                pending.clear();
            }
            if (expected != UINT64_MAX && response.bytes == expected)
                return true;
        } while (more());
        if (expected != UINT64_MAX && response.bytes != expected) {
            error = "Download parcial; NRO instalado preservado";
            return false;
        }
        return error.empty() && !cancel;
    }
    error = "Redirecionamentos em excesso";
    return false;
}
} // namespace sfm::updates
