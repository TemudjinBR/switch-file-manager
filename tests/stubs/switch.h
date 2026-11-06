#pragma once
// Apenas ciclo de vida libnx para executar o servidor FTP com sockets POSIX no host.
// Não simula hardware, Atmosphère, SSL ou desempenho do Nintendo Switch.
// Reproduz a recusa de segunda inicialização do devoptab soc: no libnx.
#include <atomic>
using Result = unsigned;
inline std::atomic<unsigned> socketInitCalls{0}, socketExitCalls{0}, socketFailNext{0};
inline bool socketServiceUp = false;
inline bool R_FAILED(Result result) { return result != 0; }
inline Result socketInitializeDefault() {
    ++socketInitCalls;
    if (unsigned error = socketFailNext.exchange(0)) return error;
    if (socketServiceUp) return (7u << 9) | 345u; // LibnxError_AlreadyInitialized / Module_Libnx
    socketServiceUp = true;
    return 0;
}
inline void socketExit() { ++socketExitCalls; socketServiceUp = false; }
