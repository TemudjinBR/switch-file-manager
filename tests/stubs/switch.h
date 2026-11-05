#pragma once
// Apenas ciclo de vida libnx para executar o servidor FTP com sockets POSIX no host.
// Não simula hardware, Atmosphère, SSL ou desempenho do Nintendo Switch.
using Result = int;
inline bool R_FAILED(Result result) { return result != 0; }
inline Result socketInitializeDefault() { return 0; }
inline void socketExit() {}
