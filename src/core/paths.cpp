#include "core/paths.h"
namespace sfm::paths {
std::string join(const std::string& a, const std::string& b) {
    if (!a.empty() && a.back() == '/') return a + b;
    return a + "/" + b;
}

std::string baseName(std::string p) {
    while (p.size() > 1 && p.back() == '/') p.pop_back();
    size_t i = p.find_last_of('/');
    return i == std::string::npos ? p : p.substr(i + 1);
}

std::string parentOf(std::string s) {
    while (s.size() > 1 && s.back() == '/') s.pop_back();
    size_t i = s.find_last_of('/');
    if (i == std::string::npos) return "sdmc:/";
    std::string r = s.substr(0, i + 1);
    if (r.size() > 6) r.pop_back();
    if (r.size() < 6) return "sdmc:/";
    return r;
}

}
