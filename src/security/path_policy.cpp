#include "security/paths.h"
#include <cstring>
namespace sfm::security {
bool isCritical(std::string p) {
    while (p.size() > 1 && p.back() == '/') p.pop_back();
    static const char* crit[] = {"sdmc:/atmosphere", "sdmc:/bootloader", "sdmc:/emuMMC", "sdmc:/Nintendo", "sdmc:/switch"};
    for (auto c : crit) if (p == c) return true;
    return false;
}

// Sanitização FAT/exFAT; conserva caracteres UTF-8 completos ao limitar o nome.
static std::string cleanComp(std::string name) {
    for (auto& c : name)
        if (strchr("\\/:*?\"<>|", c) || static_cast<unsigned char>(c) < 32) c = '_';
    if (name.size() > 200) {
        size_t end = 200;
        while (end > 0 && (static_cast<unsigned char>(name[end]) & 0xC0) == 0x80) --end;
        name.resize(end);
    }
    while (!name.empty() && (name.back() == '.' || name.back() == ' ')) name.pop_back();
    return name;
}

std::string safeName(std::string name) {
    name = cleanComp(name);
    return name.empty() ? std::string("sem_nome") : name;
}

bool validName(const std::string& name) {
    if (name.empty() || name == "." || name == ".." || name.size() > 255) return false;
    if (name.back() == '.' || name.back() == ' ') return false;
    for (unsigned char c : name) if (c < 32 || c == 127) return false;
    return name.find_first_of("/\\:*?\"<>|") == std::string::npos;
}

// converte o caminho de dentro do zip em componentes seguros; false = caminho perigoso
bool safeComponents(const std::string& raw, std::vector<std::string>& out) {
    std::string s = raw;
    for (auto& c : s) if (c == '\\') c = '/';
    if (!s.empty() && s[0] == '/') return false;                       // caminho absoluto
    if (s.size() >= 2 && s[1] == ':') return false;                    // letra de unidade (C:)
    out.clear();
    size_t i = 0;
    while (i <= s.size()) {
        size_t e = s.find('/', i);
        if (e == std::string::npos) e = s.size();
        std::string comp = s.substr(i, e - i);
        i = e + 1;
        if (comp.empty() || comp == ".") continue;
        if (comp == "..") return false;                                // tentativa de sair da pasta
        std::string c = cleanComp(comp);
        if (c.empty()) c = "_";
        out.push_back(c);
    }
    return !out.empty();
}

}
