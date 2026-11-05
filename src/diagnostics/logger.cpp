#include "sfm_diag.h"
#include "core/paths.h"
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <sys/stat.h>

namespace sfm_diag {
namespace {
std::mutex lock;
std::atomic<int> lastError{0};
constexpr size_t maxLogBytes = 1024 * 1024;

bool failure(int code) {
    if (!code) code = EIO;
    if (lastError.exchange(code) != code)
        fprintf(stderr, "Switch File Manager: falha no log (errno=%d)\n", code);
    return false;
}

bool directory(const char* path) {
    if (mkdir(path, 0777) == 0) return true;
    if (errno != EEXIST) return false;
    struct stat info{};
    if (stat(path, &info) == 0 && S_ISDIR(info.st_mode)) return true;
    errno = ENOTDIR;
    return false;
}

std::string oneLine(const std::string& input, size_t limit) {
    std::string out = input.substr(0, limit);
    for (char& c : out) if (static_cast<unsigned char>(c) < 32 || c == 127) c = ' ';
    return out;
}
}

int lastErrorCode() { return lastError.load(); }

bool write(const char* category, const std::string& message) {
    std::lock_guard<std::mutex> guard(lock);
    for (const auto* path : {"sdmc:/switch", "sdmc:/switch/Switch File Manager", sfm::paths::kLogDirectory})
        if (!directory(path)) return failure(errno);
    struct stat info{};
    const char* path = sfm::paths::kLogFile;
    if (stat(path, &info) == 0 && info.st_size >= static_cast<off_t>(maxLogBytes)) {
        const std::string previous = std::string(path) + ".1";
        if (remove(previous.c_str()) != 0 && errno != ENOENT) return failure(errno);
        if (rename(path, previous.c_str()) != 0) return failure(errno);
    }
    FILE* file = fopen(path, "ab");
    if (!file) return failure(errno);
    time_t now = time(nullptr);
    struct tm local{};
    char stamp[32] = "sem-data";
    if (localtime_r(&now, &local)) strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &local);
    const std::string cat = oneLine(category ? category : "APP", 32);
    const std::string line = oneLine(message, 4096);
    bool ok = fprintf(file, "[%s] [%s] %s\n", stamp, cat.c_str(), line.c_str()) >= 0;
    int error = ok ? 0 : errno;
    if (fflush(file) != 0) { ok = false; error = errno; }
    if (fclose(file) != 0) { ok = false; error = errno; }
    if (!ok) return failure(error);
    lastError = 0;
    return true;
}
}
