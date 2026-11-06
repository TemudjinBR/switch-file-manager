#include "network/socket_runtime.h"
#include "sfm_diag.h"
#include <switch.h>
#include <mutex>
#include <cstdio>

namespace sfm::net {
namespace {
std::mutex lock;
bool owners[2] = {false, false};
}
bool acquire(Client client, std::string& error) {
    std::lock_guard<std::mutex> guard(lock);
    error.clear();
    unsigned index = static_cast<unsigned>(client);
    if (owners[index]) return true;
    if (!owners[0] && !owners[1]) {
        Result rc = socketInitializeDefault();
        if (R_FAILED(rc)) {
            char result[96];
            snprintf(result, sizeof result, "Result=0x%08X modulo=%u descricao=%u",
                     static_cast<unsigned>(rc), static_cast<unsigned>(rc) & 0x1ff,
                     (static_cast<unsigned>(rc) >> 9) & 0x1fff);
            error = std::string("Falha ao inicializar a rede. ") + result + ". Tente novamente.";
            sfm_diag::write("REDE", std::string("etapa=socketInitializeDefault ") + result);
#ifndef HOST_TEST
            u64 total = 0, used = 0;
            Result a = svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
            Result b = svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
            sfm_diag::write("REDE", "applet_tipo=" + std::to_string(static_cast<int>(appletGetAppletType())) +
                (R_SUCCEEDED(a) && R_SUCCEEDED(b) ? " memoria_total=" + std::to_string(total) + " memoria_usada=" + std::to_string(used) : " memoria=indisponivel"));
#endif
            // socketInitialize já desfaz a inicialização parcial no libnx. Não liberar outro dono.
            return false;
        }
        sfm_diag::write("REDE", "etapa=sockets-iniciados Result=0x00000000");
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
