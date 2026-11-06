#pragma once
#include <string>
#include <cstdint>
namespace wifi {
bool start(std::string& error);
void stop();
bool running();
std::string diagnostics();
#ifdef HOST_TEST
void testFailure(const std::string& stage, int code);
#endif
std::string url();
uint64_t bytesTransferred();
unsigned filesCompleted();
uint64_t currentBytes();
unsigned activeClients();
unsigned directoryListings();
unsigned fileChanges();
std::string lastActivity();
}
