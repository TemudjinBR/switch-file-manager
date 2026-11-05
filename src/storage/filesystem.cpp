#include "storage/filesystem.h"
#include "core/paths.h"
#include <dirent.h>
#include <cerrno>
#include <sys/stat.h>
namespace sfm::storage {
using sfm::paths::join;
bool pathExists(const std::string& p) { struct stat st; return stat(p.c_str(), &st) == 0; }
bool isDir(const std::string& p) { struct stat st; return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode); }

std::string uniqueName(const std::string& p) {
    if (!pathExists(p)) return p;
    size_t dot = p.find_last_of('.'), sl = p.find_last_of('/');
    std::string base = p, ext;
    if (dot != std::string::npos && (sl == std::string::npos || dot > sl + 1)) {
        base = p.substr(0, dot);
        ext = p.substr(dot);
    }
    for (int i = 1; i < 1000; i++) {
        std::string c = base + " (" + std::to_string(i) + ")" + ext;
        if (!pathExists(c)) return c;
    }
    return p;
}

bool readDirectory(const std::string& path, std::vector<std::string>& names, std::string& error) {
    names.clear();
    DIR* dir = opendir(path.c_str());
    if (!dir) { error = "Não foi possível ler a pasta: " + path; return false; }
    errno = 0;
    while (dirent* entry = readdir(dir)) {
        std::string name = entry->d_name;
        if (name != "." && name != "..") names.push_back(name);
        errno = 0;
    }
    int code = errno;
    if (closedir(dir) != 0) code = errno;
    if (code) { error = "Falha durante a leitura da pasta: " + path; return false; }
    return true;
}

std::vector<std::string> listNames(const std::string& path) {
    std::vector<std::string> names;
    std::string error;
    readDirectory(path, names, error);
    return names;
}

uint64_t treeSize(const std::string& p) {
    struct stat st;
    if (stat(p.c_str(), &st) != 0) return 0;
    if (!S_ISDIR(st.st_mode)) return (uint64_t)st.st_size;
    uint64_t t = 0;
    for (auto& n : listNames(p)) t += treeSize(join(p, n));
    return t;
}

}
