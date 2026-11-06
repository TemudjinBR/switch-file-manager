#include "core/paths.h"
#include "storage/filesystem.h"
#include "storage/operations.h"
#include "security/paths.h"
#include "sfm_diag.h"
#include "zipx.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <cerrno>

struct LocalCancel : sfm::storage::Progress {
    bool stop = false;
    bool cancelled() const override { return stop; }
    void update(const std::string&, uint64_t bytes, bool) override { if (bytes) stop = true; }
};
struct Cancel : zx::Progress {
    bool tick(uint64_t, const char*) override { return false; }
};

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    std::string command = argv[1];
    if (command == "paths") {
        using namespace sfm::paths;
        using namespace sfm::storage;
        using namespace sfm::security;
        if (join("sdmc:/", "a") != "sdmc:/a" || parentOf("sdmc:/a/b/") != "sdmc:/a" ||
            parentOf("sdmc:/") != "sdmc:/" || baseName("sdmc:/a/") != "a" ||
            !isCritical("sdmc:/atmosphere/") || isCritical("sdmc:/atmosphere-backup") ||
            validName("../a") || validName("..") || validName("linha\nquebrada") || validName("final.") ||
            safeName("café") != "café" || safeName(std::string(199, 'a') + "é").size() != 199) return 1;
        std::vector<std::string> parts;
        for (auto bad : {"../x", "a/../../x", "/absolute", "C:\\outside", "a\\..\\x"})
            if (safeComponents(bad, parts)) return 1;
        if (!safeComponents("a/b.txt", parts) || parts.size() != 2) return 1;
        std::ofstream("file.txt") << "abc";
        if (treeSize("file.txt") != 3 || uniqueName("file.txt") != "file (1).txt") return 1;
        std::cout << kLegacyConfigDir << '\n' << kLogFile << '\n';
        return 0;
    }
    if ((command == "copy" || command == "verify" || command == "cancel-copy") && argc == 4) {
        std::string error; LocalCancel cancel;
        bool ok = command == "verify" ? sfm::storage::verifyTreeCopy(argv[2], argv[3], nullptr, error) :
            sfm::storage::copyTree(argv[2], argv[3], command == "cancel-copy" ? &cancel : nullptr, error);
        if (!ok) std::cerr << error;
        return ok ? 0 : 1;
    }
    if (command == "remove" && argc == 3) {
        std::string error;
        if (!sfm::storage::removeTree(argv[2], nullptr, error)) { std::cerr << error; return 1; }
        return 0;
    }
    if (command == "log") {
        bool ok = sfm_diag::write("TESTE", "mensagem\nsem linha falsa");
        std::cout << ok << ' ' << sfm_diag::lastErrorCode() << '\n';
        return ok ? 0 : 1;
    }
    if (command == "parallel") {
        std::vector<std::thread> threads;
        for (int t = 0; t < 4; ++t) threads.emplace_back([t] {
            for (int n = 0; n < 100; ++n) sfm_diag::write("THREAD", std::to_string(t) + ":" + std::to_string(n));
        });
        for (auto& thread : threads) thread.join();
        return sfm_diag::lastErrorCode() ? 1 : 0;
    }
    if (command == "rotate") {
        for (int n = 0; n < 300; ++n) if (!sfm_diag::write("ROTACAO", std::string(4096, 'x'))) return 1;
        return 0;
    }
    if (command == "inspect" && argc == 3) {
        zx::Info info;
        if (!zx::inspect(argv[2], info)) { std::cerr << zx::lastError(); return 1; }
        std::cout << info.files << ' ' << info.dirs << ' ' << info.totalBytes << '\n'; return 0;
    }
    if ((command == "extract" || command == "cancel-extract") && argc == 4) {
        int skipped = 0; Cancel cancel;
        bool ok = zx::extract(argv[2], argv[3], command == "extract" ? nullptr : &cancel, skipped);
        std::cout << skipped << '\n';
        if (!ok) std::cerr << zx::lastError();
        return ok ? 0 : 1;
    }
    if (command == "compress" && argc == 4) {
        if (!zx::compress({argv[2]}, argv[3], 6, nullptr)) { std::cerr << zx::lastError(); return 1; }
        return 0;
    }
    return 2;
}
