#pragma once
#include <string>

namespace sfm::net {
enum class Client { Ftp, Drive };
// Uma posse por consumidor. Repetir acquire/release é seguro; o último libera o SDK.
bool acquire(Client client, std::string& error);
void release(Client client);
bool localAddress(unsigned& address, std::string& error);
std::string diagnostics();
#ifdef HOST_TEST
void testNetwork(bool available);
#endif
}
