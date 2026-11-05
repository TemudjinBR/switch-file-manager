#pragma once
#include <string>
#include <cstdint>
namespace wifi {
bool start(std::string& error);
void stop();
bool running();
std::string url();
uint64_t bytesTransferred();
unsigned filesCompleted();
uint64_t currentBytes();
}
