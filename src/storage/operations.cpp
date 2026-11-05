#include "storage/operations.h"
#include "storage/filesystem.h"
#include "core/paths.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
namespace sfm::storage {
using sfm::paths::join;
using sfm::paths::baseName;
static bool cancelled(Progress* progress) { return progress && progress->cancelled(); }
static void update(Progress* progress, const std::string& name, uint64_t bytes = 0, bool completed = false) {
    if (progress) progress->update(name, bytes, completed);
}
static bool copyFile(const std::string& s, const std::string& d, Progress* progress, std::string& error, bool& created) {
    FILE* in = fopen(s.c_str(), "rb");
    if (!in) { error = "Não foi possível abrir: " + baseName(s); return false; }
    const size_t BS = 1 << 20;
    char* buf = (char*)malloc(BS);
    if (!buf) { fclose(in); error = "Memória insuficiente."; return false; }
    // Exclusividade: uma mudança no destino durante a operação não deve sobrescrever dados.
    if (pathExists(d)) {
        free(buf); fclose(in); error = "Destino já existe: " + baseName(d); return false;
    }
    int fd = open(d.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) { free(buf); fclose(in); error = "Não foi possível criar: " + baseName(d); return false; }
    created = true;
    FILE* out = fdopen(fd, "wb");
    if (!out) { close(fd); remove(d.c_str()); free(buf); fclose(in); error = "Falha ao abrir destino da cópia."; return false; }
    bool ok = true;
    size_t n;
    while ((n = fread(buf, 1, BS, in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) { ok = false; error = "Falha ao gravar (cartão cheio?)."; break; }
        update(progress, baseName(s), n);
        if (cancelled(progress)) { ok = false; error = "Cópia interrompida pelo usuário."; break; }
    }
    if (ok && fflush(out) != 0) { ok = false; error = "Falha ao descarregar dados no destino."; }
    if (ferror(in)) { ok = false; error = "Falha ao ler: " + baseName(s); }
    free(buf);
    if (fclose(in) != 0) { ok = false; error = "Falha ao fechar arquivo de origem: " + baseName(s); }
    // fclose pode detectar erro de gravação mesmo quando fwrite não detectou.
    if (fclose(out) != 0) { ok = false; error = "Falha ao finalizar gravação: " + baseName(d); }
    if (!ok) {
        // Se não for possível limpar, avisar: nunca declarar que o destino está íntegro.
        const std::string cause = error;
        if (remove(d.c_str()) != 0)
            error = cause + " Arquivo parcial permaneceu em: " + d;
    } else update(progress, baseName(s), 0, true);
    return ok;
}

static bool copyTreeImpl(const std::string& s, const std::string& d, Progress* progress, std::string& error, bool& created) {
    if (cancelled(progress)) return false;
    update(progress, baseName(s));
    if (isDir(s)) {
        if (mkdir(d.c_str(), 0777) != 0) {
            error = "Não foi possível criar a pasta: " + baseName(d);
            return false;
        }
        created = true;
        std::vector<std::string> names;
        if (!readDirectory(s, names, error)) return false;
        for (auto& n : names)
            if (!copyTreeImpl(join(s, n), join(d, n), progress, error, created)) return false;
        return true;
    }
    return copyFile(s, d, progress, error, created);
}

// Verificação byte a byte, com memória limitada, antes de remover a origem.
// Não é uma verificação criptográfica; compara integralmente os dados lidos.
static bool verifyFileCopy(const std::string& source, const std::string& target, Progress* progress, std::string& error) {
    FILE* a = fopen(source.c_str(), "rb");
    if (!a) { error = "Falha ao abrir origem para conferir: " + baseName(source); return false; }
    FILE* b = fopen(target.c_str(), "rb");
    if (!b) { fclose(a); error = "Falha ao abrir destino para conferir: " + baseName(target); return false; }
    const size_t chunk = 64 * 1024;
    char* x = (char*)malloc(chunk);
    char* y = (char*)malloc(chunk);
    if (!x || !y) {
        free(x); free(y); fclose(a); fclose(b);
        error = "Memória insuficiente para verificar a cópia.";
        return false;
    }
    bool ok = true;
    while (true) {
        if (cancelled(progress)) { ok = false; break; }
        size_t nx = fread(x, 1, chunk, a);
        size_t ny = fread(y, 1, chunk, b);
        if (ferror(a) || ferror(b) || nx != ny || (nx && memcmp(x, y, nx) != 0)) {
            ok = false; break;
        }
        if (!nx) break;
        update(progress, "Verificando: " + baseName(source));
    }
    free(x); free(y);
    if (fclose(a) != 0) ok = false;
    if (fclose(b) != 0) ok = false;
    if (!ok && !cancelled(progress)) error = "Cópia diferente da origem ou erro de leitura: " + baseName(source);
    return ok;
}

bool verifyTreeCopy(const std::string& source, const std::string& target, Progress* progress, std::string& error) {
    if (cancelled(progress)) return false;
    bool dir = isDir(source);
    if (dir != isDir(target)) {
        error = "Tipo diferente no destino: " + baseName(source);
        return false;
    }
    if (!dir) return verifyFileCopy(source, target, progress, error);
    std::vector<std::string> names, copied;
    if (!readDirectory(source, names, error) || !readDirectory(target, copied, error)) return false;
    std::sort(names.begin(), names.end());
    std::sort(copied.begin(), copied.end());
    if (names != copied) {
        error = "Conteúdo diferente na pasta: " + baseName(source);
        return false;
    }
    for (const auto& n : names)
        if (!verifyTreeCopy(join(source, n), join(target, n), progress, error)) return false;
    return true;
}

bool removeTree(const std::string& p, Progress* progress, std::string& error) {
    if (isDir(p)) {
        // Não ocultar falhas em filhos: uma remoção parcial deve ser informada.
        bool ok = true;
        std::vector<std::string> names;
        if (!readDirectory(p, names, error)) return false;
        for (auto& n : names)
            if (!removeTree(join(p, n), progress, error)) ok = false;
        update(progress, baseName(p));
        if (rmdir(p.c_str()) != 0) ok = false;
        if (!ok) error = "Não foi possível apagar completamente: " + baseName(p);
        return ok;
    }
    update(progress, baseName(p));
    if (remove(p.c_str()) != 0) {
        error = "Não foi possível apagar: " + baseName(p);
        return false;
    }
    return true;
}

bool copyTree(const std::string& source, const std::string& target, Progress* progress, std::string& error) {
    bool created = false;
    bool ok = copyTreeImpl(source, target, progress, error, created);
    if (!ok && created && pathExists(target)) {
        std::string cleanup;
        if (!removeTree(target, nullptr, cleanup)) error += " ATENÇÃO: restos da cópia podem permanecer em " + target;
    }
    return ok;
}
}
