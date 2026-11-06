#include "network/socket_runtime.h"
#include "sfm_diag.h"
#include <switch.h>
#include <mutex>
#include <cstdio>
#include <arpa/inet.h>
#include <unistd.h>

namespace sfm::net {
namespace {
std::mutex lock;
bool owners[2] = {false, false};
unsigned sessionCount = 0;
std::string detail = "sockets=nao-inicializados";
#ifdef HOST_TEST
bool networkAvailable = true;
#endif
std::string context() {
#ifdef HOST_TEST
    return "modo=host-simulado";
#else
    u64 total = 0, used = 0;
    Result a = svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
    Result b = svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
    return "applet_tipo=" + std::to_string(static_cast<int>(appletGetAppletType())) +
        (R_SUCCEEDED(a) && R_SUCCEEDED(b) ? " memoria_total=" + std::to_string(total) + " memoria_usada=" + std::to_string(used) : " memoria=indisponivel");
#endif
}
std::string resultText(Result rc) {
    char out[128];
    snprintf(out, sizeof out, "Result=0x%08X modulo=%u descricao=%u", unsigned(rc), unsigned(rc)&511, (unsigned(rc)>>9)&8191);
    return out;
}
}
bool acquire(Client client, std::string& error) {
    std::lock_guard<std::mutex> guard(lock);
    error.clear();
    unsigned index = static_cast<unsigned>(client);
    if (owners[index]) return true;
    if (!owners[0] && !owners[1]) {
        // Quatro clientes FTP + listener + Drive + encerramento/consulta concorrente.
        SocketInitConfig config = *socketGetDefaultInitConfig();
        config.num_bsd_sessions = 8;
        Result rc = socketInitialize(&config);
        std::string fallback;
        const unsigned value = unsigned(rc) & 0x3fffffu;
        // Recusar erros desconhecidos; voltar ao orçamento original somente em falta de recursos.
        if (value == ((104u << 9) | 1u) || value == ((105u << 9) | 1u) ||
            value == ((132u << 9) | 1u) || value == ((2u << 9) | 345u)) {
            fallback = "\nmodo=recursos-reduzidos causa=" + resultText(rc);
            sfm_diag::write("REDE", "etapa=bsd-orcamento fallback=3 " + resultText(rc));
            config.num_bsd_sessions = 3;
            rc = socketInitialize(&config);
        }
        if (R_FAILED(rc)) {
            detail = "etapa=sockets-inicializar\n" + resultText(rc) + "\n" + context();
            error = detail;
            sfm_diag::write("REDE", detail);
            // socketInitialize já desfaz a inicialização parcial no libnx. Não liberar outro dono.
            return false;
        }
        sessionCount = config.num_bsd_sessions;
        detail = "etapa=sockets-iniciados Result=0x00000000\n" + context() + fallback;
        sfm_diag::write("REDE", detail);
    }
    owners[index] = true;
    return true;
}
void release(Client client) {
    std::lock_guard<std::mutex> guard(lock);
    unsigned index = static_cast<unsigned>(client);
    if (!owners[index]) return;
    owners[index] = false;
    if (!owners[0] && !owners[1]) {
        socketExit();
        sfm_diag::write("REDE", "etapa=sockets-encerrados");
    }
}
}

namespace sfm::net {
std::string diagnostics() {
    std::lock_guard<std::mutex> guard(lock);
    return detail + (sessionCount == 3 ? "\nmodo=recursos-reduzidos" : "") + std::string("\nbsd_sessoes=") + std::to_string(sessionCount) + " FTP=" + std::to_string(owners[0]) + " Drive=" + std::to_string(owners[1]);
}
bool localAddress(unsigned& address, std::string& error) {
    std::lock_guard<std::mutex> guard(lock);
#ifdef HOST_TEST
    if (!networkAvailable) { error = "etapa=wifi-ip\nerrno=100 (rede simulada indisponivel)"; return false; }
    address = htonl(INADDR_LOOPBACK);
    detail = "etapa=wifi-ip modo=host-simulado";
    return true;
#else
    Result init = nifmInitialize(NifmServiceType_User);
    if (R_FAILED(init)) {
        unsigned fallback = static_cast<unsigned>(gethostid());
        detail = "etapa=nifm-inicializar\n" + resultText(init) + "\n" + context();
        if (fallback && fallback != 0xffffffffu && (ntohl(fallback) >> 24) != 127) {
            address = fallback; detail += "\nfonte_ip=bsd-gethostid"; sfm_diag::write("REDE", detail); return true;
        }
        error = detail; return false;
    }
    NifmInternetConnectionType type{};
    NifmInternetConnectionStatus status{};
    u32 strength = 0, ip = 0;
    Result state = nifmGetInternetConnectionStatus(&type, &strength, &status);
    Result query = nifmGetCurrentIpAddress(&ip);
    nifmExit();
    detail = "etapa=wifi-ip\n" + resultText(query) + "\nstatus=" + std::to_string(int(status)) +
        " consulta_status=" + resultText(state) + "\n" + context();
    sfm_diag::write("REDE", detail);
    if (R_FAILED(query) || !ip || ip == 0xffffffffu) {
        unsigned fallback = static_cast<unsigned>(gethostid());
        if (fallback && fallback != 0xffffffffu && (ntohl(fallback) >> 24) != 127) {
            address = fallback; detail += "\nfonte_ip=bsd-gethostid"; sfm_diag::write("REDE", detail); return true;
        }
        error = detail + "\nSem endereco local. Conecte o Wi-Fi e tente novamente."; return false;
    }
    // IP local basta para FTP LAN, mesmo sem acesso a um servidor da internet.
    address = ip; return true;
#endif
}
#ifdef HOST_TEST
void testNetwork(bool available) { std::lock_guard<std::mutex> guard(lock); networkAvailable = available; }
#endif
}
