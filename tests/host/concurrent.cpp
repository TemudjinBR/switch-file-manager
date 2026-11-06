#include "gdrive.h"
#include "wifi_transfer.h"
#include <switch.h>
#include <poll.h>
#include <unistd.h>
#include <chrono>
#include <iostream>
#include <thread>

// Uma thread chamadora do Drive/UI e os workers FTP reais, no mesmo processo.
static bool readCommand(std::string& command) {
    static std::string line;
    pollfd input{STDIN_FILENO, POLLIN, 0};
    while (poll(&input, 1, 0) > 0 && (input.revents & POLLIN)) {
        char c;
        if (read(STDIN_FILENO, &c, 1) != 1) return false;
        if (c == '\n') { command = line; line.clear(); return true; }
        line += c;
    }
    return false;
}
static void control(const std::string& command) {
    std::string error;
    if (command == "ftp-stop") wifi::stop();
    if (command == "ftp-start") wifi::start(error);
    if (command == "drive-stop") gd::netExit();
    std::cout << "LIVE " << command << " ftp=" << wifi::running() << " url=" << wifi::url() << " " << error << std::endl;
}
int main(int argc, char** argv) {
    if (argc != 4) return 2;
    gd::testPort = std::atoi(argv[1]); gd::testDir = argv[2];
    std::string error;
    bool first = std::string(argv[3]) == "ftp-first";
    if (first && !wifi::start(error)) return 3;
    if (!gd::netInit() || !gd::loadSavedLogin()) { std::cerr << gd::lastError(); return 4; }
    if (!first && !wifi::start(error)) return 5;
    std::cout << "READY " << wifi::url() << " sessions=" << socketSessionCount << std::endl;
    gd::setNetworkPump([] {
        std::string command;
        if (readCommand(command)) control(command);
        return true;
    });
    for (;;) {
        std::string command;
        if (!readCommand(command)) { std::this_thread::sleep_for(std::chrono::milliseconds(5)); continue; }
        if (command == "quit") break;
        bool ok = false;
        if (command == "list") { std::vector<gd::Item> items; ok = gd::listFolder("root", items); }
        else if (command.rfind("download=", 0) == 0) { gd::Item item; item.id="test-file"; ok=gd::downloadFile(item,command.substr(9),nullptr); }
        else if (command.rfind("upload=", 0) == 0) ok=gd::uploadFile(command.substr(7),"root","fixture.bin",nullptr);
        else { control(command); continue; }
        std::cout << "DONE " << command << " ok=" << ok << " logged=" << gd::isLoggedIn() << std::endl;
    }
    gd::setNetworkPump({}); wifi::stop(); gd::netExit();
}
