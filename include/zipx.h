// ZIP: extrair e compactar (usa a biblioteca miniz, domínio público)
#pragma once
#include <stdint.h>
#include <string>
#include <vector>

namespace zx {

// bytes = quantidade processada agora; name = item atual. Retorne false para cancelar.
struct Progress {
    virtual bool tick(uint64_t bytes, const char* name) = 0;
    virtual ~Progress() {}
};

struct Info {
    uint64_t totalBytes = 0;  // tamanho descompactado
    uint32_t files = 0;
    uint32_t dirs = 0;
    bool encrypted = false;   // tem itens protegidos por senha
};

std::string lastError();

bool inspect(const std::string& zipPath, Info& out);
// Extrai para destDir (a pasta é criada). skipped recebe quantos itens foram ignorados
// (caminhos perigosos, protegidos por senha ou formato não suportado).
bool extract(const std::string& zipPath, const std::string& destDir, Progress* pr, int& skipped);
// Compacta os caminhos (arquivos ou pastas) em zipPath. level: 0 = sem compressão, 1 = rápido, 6 = normal.
bool compress(const std::vector<std::string>& paths, const std::string& zipPath, int level, Progress* pr);

}  // namespace zx
