#pragma once
#include <chrono>
#include <cstdint>
#include <functional>
#include <thread>
namespace sfm::net {
// Repete apenas WouldBlock. Erros de certificado/protocolo nunca viram sucesso.
inline uint32_t waitIo(const std::function<uint32_t()>& call, const std::function<bool()>& active,
                       unsigned timeoutMs, unsigned sleepMs = 20) {
    constexpr uint32_t wouldBlock = (204u << 9) | 123u;
    constexpr uint32_t timedOut = (205u << 9) | 123u;
    constexpr uint32_t aborted = (206u << 9) | 123u;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    for (;;) {
        if (active && !active()) return aborted;
        uint32_t result = call();
        if ((result & 0x3fffffu) != wouldBlock) return result;
        if (std::chrono::steady_clock::now() >= deadline) return timedOut;
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    }
}
}
