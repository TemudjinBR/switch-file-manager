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
    if (command == "logout" || command == "forget" || command == "keep-on" || command == "keep-off") {
        bool ok = command == "logout" ? gd::logout() : command == "forget" ? gd::forgetAccount() : gd::setKeepConnected(command == "keep-on");
        if (!ok) std::cerr << gd::lastError();
        return ok ? 0 : 1;
    }
    if (command == "serve") {
        if (argc < 5) return 2;
        gd::testLoginPort = std::atoi(argv[4]);
        for (int i = 4; i < argc; ++i) if (std::string(argv[i]) == "--restore") gd::loadSavedLogin();
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
                if (action == "state") std::cout << "state=" << gd::connectionLabel() << " keep=" << gd::keepConnected() << " name=" << gd::accountName() << std::endl;
                if (action == "cancel") { gd::loginServerStop(); std::cout << "cancelled" << std::endl; }
                if (action == "switch") { gd::loginServerStop(); bool ok = gd::loginServerStart(url); std::cout << "switch=" << ok << std::endl; }
                if (action == "list") { std::vector<gd::Item> items; bool ok = gd::listFolder("root", items); std::cout << "list=" << ok << std::endl; }
            }
            gd::loginServerPoll();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        gd::loginServerStop(); return 0;
    }
    if (!(command == "reconnect" ? gd::reconnectSavedLogin() : gd::loadSavedLogin())) {
        std::cerr << gd::lastError();
        std::cout << "logged=" << gd::isLoggedIn() << '\n' << "state=" << gd::connectionLabel() << '\n'; return 1;
    }
    bool ok = command == "reconnect";
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
    if (!ok) {
        std::cerr << gd::lastError();
        std::cout << "logged=" << gd::isLoggedIn() << " state=" << gd::connectionLabel() << '\n';
    }
    return ok ? 0 : 1;
}
