#include "wifi_transfer.h"
#include <iostream>
#include <string>
int main() {
    std::string error;
    if (!wifi::start(error)) { std::cerr << error; return 1; }
    std::cout << wifi::url() << std::endl;
    std::string stop;
    std::getline(std::cin, stop);
    wifi::stop();
    return 0;
}
