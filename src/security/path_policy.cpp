#include "security/paths.h"
#include <cstring>
namespace sfm::security {
bool isCritical(std::string p) {
    while (p.size() > 1 && p.back() == '/') p.pop_back();
    static const char* crit[] = {"sdmc:/atmosphere", "sdmc:/bootloader", "sdmc:/emuMMC", "sdmc:/Nintendo", "sdmc:/switch"};
    for (auto c : crit) if (p == c) return true;
    return false;
}

std::string safeName(std::string n) {
    for (auto& c : n)
        if (strchr("\\/:*?\"<>|", c) || (unsigned char)c < 32) c = '_';
    while (!n.empty() && (n.back() == '.' || n.back() == ' ')) n.pop_back();
    while (n.size() > 200) n.pop_back();
    while (!n.empty() && (((unsigned char)n.back() & 0xC0) == 0x80 || ((unsigned char)n.back() & 0xC0) == 0xC0)) n.pop_back();
    return n.empty() ? std::string("sem_nome") : n;
}

bool validName(const std::string& n) {
    if (n.empty() || n == "." || n == "..") return false;
    return n.find_first_of("/\\:*?\"<>|") == std::string::npos;
}

// limpa um componente de caminho para o cartão SD (FAT/exFAT)
static std::string cleanComp(std::string n) {
    for (auto& c : n)
        if (strchr("\\/:*?\"<>|", c) || (unsigned char)c < 32) c = '_';
    while (!n.empty() && (n.back() == '.' || n.back() == ' ')) n.pop_back();
    while (n.size() > 200) n.pop_back();
    while (!n.empty() && (((unsigned char)n.back() & 0xC0) == 0x80 || ((unsigned char)n.back() & 0xC0) == 0xC0)) n.pop_back();
    return n;
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
