#include "updates/update_core.h"
#include "wifi_transfer.h"
#include "updates/update_http.h"
#include "updates/update_service.h"
#include <fstream>
#include <iostream>
using namespace sfm::updates;
static void copy(const std::string &from, const std::string &to) {
    std::ifstream input(from, std::ios::binary);
    std::ofstream out(to, std::ios::binary);
    out << input.rdbuf();
}
int main(int argc, char **argv) {
    if (argc < 2)
        return 2;
    std::string mode = argv[1], error;
    if (mode == "compare") {
        if (argc < 4 || !version(argv[2]) || !version(argv[3]))
            return 2;
        std::cout << compareVersions(argv[2], argv[3]);
        return 0;
    }
    if (mode == "hash") {
        std::string hash;
        uint64_t size = 0;
        if (!hashFile(argv[2], hash, size, error))
            return 1;
        std::cout << hash;
        return 0;
    }
    if (mode == "service") {
        httpTestPort = std::atoi(argv[2]);
        Service service;
        service.initialize(officialTarget, "1.0.0", "v1.0.0-aaaaaaaaaaaa");
        std::cout << "ready" << std::endl;
        std::string command;
        while (std::getline(std::cin, command)) {
            if (command == "ftp-on") { std::string failure; wifi::start(failure); std::cout << "ftp=" << wifi::url() << " " << failure << std::endl; continue; }
            if (command == "quit")
                break;
            bool ok = true;
            if (command == "check")
                ok = service.check(false, error);
            if (command == "download")
                ok = service.download(error);
            if (command == "cancel")
                service.cancel();
            if (command == "auto-off")
                ok = service.configure(service.prefs().channel, false, error);
            if (command.rfind("channel=", 0) == 0)
                ok = service.configure(command.substr(8), service.prefs().automatic, error);
            auto s = service.status();
            auto p = service.prefs();
            std::cout << "ok=" << ok << " busy=" << s.busy << " ready=" << s.ready << " channel=" << p.channel
                      << " auto=" << p.automatic << " version=" << s.available.version << " state=" << s.state
                      << " error=" << s.error << " action=" << error << std::endl;
        }
        service.stop();
        wifi::stop();
        return 0;
    }
    if (mode == "http") {
        httpTestPort = std::atoi(argv[2]);
        std::atomic<bool> cancel{false};
        Response response;
        std::string body;
        bool ok = getHttps(
            argv[3], 100000,
            [&](const uint8_t *data, size_t n) {
                body.append(reinterpret_cast<const char *>(data), n);
                return true;
            },
            cancel, response, error);
        std::cout << body;
        std::cerr << error;
        return ok ? 0 : 1;
    }
    if (mode == "target") {
        bool ok = runningTarget(argv[2], argv[3], argv[4], error);
        std::cerr << error;
        return ok ? 0 : 1;
    }
    if (mode == "manifest") {
        std::string text;
        if (!readSmall(argv[2], text, error))
            return 1;
        Manifest m;
        bool ok = parseManifest(text, argv[3], m, error);
        std::cout << m.version;
        std::cerr << error;
        return ok ? 0 : 1;
    }
    if (mode == "prepare") {
        std::string text;
        Manifest m;
        if (!readSmall(argv[2], text, error) || !parseManifest(text, "experimental", m, error)) {
            std::cerr << error;
            return 1;
        }
        Transaction t;
        t.manifest = m;
        bool ok = space(officialDirectory, m.app.size + m.helper.size, error) &&
                  reserve(t, officialDirectory, error);
        if (ok) {
            copy(argv[3], t.directory + "/new.nro");
            copy(argv[4], t.directory + "/helper.nro");
            ok = prepareTransaction(t, error);
        }
        std::cout << t.directory;
        std::cerr << error;
        return ok ? 0 : 1;
    }
    Transaction t;
    if (!readTransaction(argv[2], t, error)) {
        std::cerr << error;
        return 1;
    }
    if (argc > 3) {
        std::string hook = argv[3];
        bool crash = hook.rfind("crash=", 0) == 0;
        testFault(hook.substr(hook.find('=') + 1), 28, crash);
    }
    bool ok = install(t, mode == "rollback", error);
    std::cerr << error;
    return ok ? 0 : 1;
}
