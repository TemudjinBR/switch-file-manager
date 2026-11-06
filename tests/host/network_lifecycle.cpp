#include "gdrive.h"
#include "wifi_transfer.h"
#include <switch.h>
#include <iostream>

int main() {
    std::string action, error;
    while (std::getline(std::cin, action)) {
        if (action == "quit") break;
        if (action == "fail") socketFailNext = 0x345;
        if (action == "drive-on") std::cout << "drive=" << gd::netInit() << " " << gd::lastError() << '\n';
        if (action == "drive-off") gd::netExit();
        if (action == "ftp-on") {
            bool ok = wifi::start(error);
            std::cout << (ok ? wifi::url() : error) << '\n';
        }
        if (action == "ftp-off") wifi::stop();
        std::cout << "init=" << socketInitCalls << " exit=" << socketExitCalls << " up=" << socketServiceUp << std::endl;
    }
    wifi::stop(); gd::netExit();
}
