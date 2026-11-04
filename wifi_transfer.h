#pragma once
#include <string>
namespace wifi {
bool start(std::string& error);
void stop();
bool running();
std::string url();
}
