#include "gdrive.h"
#include "network/socket_runtime.h"
#include "wifi_transfer.h"
#include <switch.h>
#include <iostream>

int main() {
    std::string action, error;
    while (std::getline(std::cin, action)) {
        if (action == "quit") break;
        if (action == "network-off") sfm::net::testNetwork(false);
        if (action == "network-on") sfm::net::testNetwork(true);
        if (action.rfind("fault-", 0) == 0) wifi::testFailure(action.substr(6), 11);
        if (action == "resource-fail") socketFailNext = (104u << 9) | 1u;
        if (action == "session-count") std::cout << "sessions=" << socketSessionCount << " " << sfm::net::diagnostics() << "\n";
        if (action == "fail") socketFailNext = 0x345;
        if (action == "drive-on") std::cout << "drive=" << gd::netInit() << " " << gd::lastError() << '\n';
        if (action == "drive-off") gd::netExit();
        if (action == "ftp-on") {
            bool ok = wifi::start(error);
            std::cout << (ok ? wifi::url() : error) << '\n';
        }
        if (action == "diagnostic") std::cout << wifi::diagnostics() << '\n';
        if (action == "ftp-off") wifi::stop();
        std::cout << "init=" << socketInitCalls << " exit=" << socketExitCalls << " up=" << socketServiceUp << std::endl;
    }
    wifi::stop(); gd::netExit();
}
