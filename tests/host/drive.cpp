#include "gdrive.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <cstdlib>
int main(int argc, char** argv) {
    if (argc < 4) return 2;
    gd::testPort = std::atoi(argv[1]);
    gd::testDir = argv[2];
    std::string command = argv[3];
    if (command == "serve") {
        gd::testLoginPort = std::atoi(argv[4]);
        std::string url;
        if (!gd::loginServerStart(url)) { std::cerr << gd::lastError(); return 1; }
        std::cout << url << std::endl;
        std::atomic<bool> done{false};
        std::thread worker([&] { while (!done) { gd::loginServerPoll(); std::this_thread::sleep_for(std::chrono::milliseconds(5)); } });
        std::string stop; std::getline(std::cin, stop);
        done = true; worker.join(); gd::loginServerStop(); return 0;
    }
    if (!gd::loadSavedLogin()) { std::cerr << gd::lastError(); return 1; }
    bool ok = false;
    if (command == "list") {
        std::vector<gd::Item> items;
        ok = gd::listFolder("root", items);
        for (const auto& item : items) std::cout << item.id << '|' << item.name << '|' << item.size << '\n';
    } else if (command == "download" && argc == 6) {
        gd::Item item; item.id = "test-file"; item.size = std::strtoull(argv[5], nullptr, 10);
        ok = gd::downloadFile(item, argv[4], nullptr);
    } else if (command == "upload" && argc == 5) {
        ok = gd::uploadFile(argv[4], "root", "upload.bin", nullptr);
    }
    if (!ok) std::cerr << gd::lastError();
    return ok ? 0 : 1;
}
