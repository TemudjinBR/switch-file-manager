#include "cloud/credential_store.h"
#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 4) return 2;
    std::string command = argv[1], root = argv[2];
    gd::persist::File file = std::string(argv[3]) == "credential" ? gd::persist::File::Credentials : gd::persist::File::Token;
    std::vector<gd::persist::Fault> faults;
    for (int i = 4; i < argc; ++i) {
        std::string value = argv[i];
        if (value == "--short") { gd::persist::testWriteLimit(2); continue; }
        if (value.compare(0, 8, "--crash=") == 0) { gd::persist::testCrashAt(value.substr(8)); continue; }
        auto separator = value.find('=');
        if (separator != std::string::npos) faults.push_back({value.substr(0, separator), std::atoi(value.c_str() + separator + 1), 1});
    }
    gd::persist::testFaults(std::move(faults));
    gd::persist::Error error;
    std::string value;
    bool ok = command == "load" ? gd::persist::load(root, file, value, error) :
              command == "ensure" ? gd::persist::ensureDirectory(root, error) :
              gd::persist::save(root, file, "fictional-refresh-new", error);
    std::cout << (ok ? "ok" : "failed") << " " << gd::persist::describe(error) << '\n';
    return ok ? 0 : 1;
}
