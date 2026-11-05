#include "gdrive.h"
#include "cloud/credential_store.h"
#include <chrono>
#include <iostream>
#include <thread>
#include <cstdlib>
#include <poll.h>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc < 4) return 2;
    gd::testPort = std::atoi(argv[1]);
    gd::testDir = argv[2];
    std::string command = argv[3];
    std::vector<gd::persist::Fault> faults;
    for (int i = 4; i < argc; ++i) {
        std::string value = argv[i];
        if (value.compare(0, 8, "--fault=") == 0) {
            value = value.substr(8);
            auto delimiter = value.find(':');
            auto skip = value.find(':', delimiter + 1);
            faults.push_back({value.substr(0, delimiter), std::atoi(value.c_str() + delimiter + 1), 1,
                              skip == std::string::npos ? 0 : std::atoi(value.c_str() + skip + 1)});
        }
    }
    gd::persist::testFaults(std::move(faults));
    if (command == "logout") {
        bool ok = gd::logout();
        if (!ok) std::cerr << gd::lastError();
        return ok ? 0 : 1;
    }
    if (command == "serve") {
        if (argc < 5) return 2;
        gd::testLoginPort = std::atoi(argv[4]);
        std::string url;
        if (!gd::loginServerStart(url)) { std::cerr << gd::lastError(); return 1; }
        std::cout << url << std::endl;
        for (;;) {
            pollfd input{STDIN_FILENO, POLLIN, 0};
            if (poll(&input, 1, 0) > 0) {
                std::string action;
                if (!std::getline(std::cin, action) || action == "stop") break;
                if (action == "status") std::cout << "logged=" << gd::isLoggedIn()
                    << " source=" << static_cast<int>(gd::lastErrorSource()) << std::endl;
                if (action == "clear-faults") gd::persist::testFaults({});
            }
            gd::loginServerPoll();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        gd::loginServerStop(); return 0;
    }
    if (!gd::loadSavedLogin()) {
        std::cerr << gd::lastError();
        std::cout << "logged=" << gd::isLoggedIn() << '\n'; return 1;
    }
    bool ok = false;
    if (command == "list") {
        std::vector<gd::Item> items;
        ok = gd::listFolder("root", items);
        for (const auto& item : items) std::cout << item.id << '|' << item.name << '|' << item.size << '\n';
    } else if (command == "download" && argc >= 6) {
        gd::Item item; item.id = "test-file"; item.size = std::strtoull(argv[5], nullptr, 10);
        ok = gd::downloadFile(item, argv[4], nullptr);
    } else if (command == "upload" && argc >= 5) {
        ok = gd::uploadFile(argv[4], "root", "upload.bin", nullptr);
    }
    if (!ok) std::cerr << gd::lastError();
    return ok ? 0 : 1;
}
