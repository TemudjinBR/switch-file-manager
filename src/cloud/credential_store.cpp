#include "cloud/credential_store.h"
#include "sfm_diag.h"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <limits>

namespace gd::persist {
namespace {
std::mutex storeLock;
constexpr size_t maxBytes = 64 * 1024;

#ifdef HOST_TEST
std::vector<Fault> faults;
unsigned writeLimit = 0;
std::string crashStage;
#endif

// Os hooks existem somente no host; toda E/S continua usando arquivos reais de teste.
bool before(const std::string& stage) {
#ifdef HOST_TEST
    if (crashStage == stage) std::_Exit(77);
    for (auto& fault : faults) {
        if (fault.stage == stage && fault.remaining > 0) {
            if (fault.skip > 0) { --fault.skip; continue; }
            --fault.remaining; errno = fault.code; return false;
        }
    }
#else
    (void)stage;
#endif
    errno = 0;
    return true;
}

bool fail(Error& error, File file, const std::string& stage, int code) {
    error.stage = stage;
    error.code = code ? code : EIO;
    sfm_diag::write(file==File::Updates?"UPDATES_SD":"DRIVE_SD", std::string("arquivo=") + filename(file) +
                    " etapa=" + stage + " errno=" + std::to_string(error.code));
    return false;
}

void recoveryFailure(Error& error, File file, const std::string& stage, int code) {
    error.recoveryStage = stage;
    error.recoveryCode = code ? code : EIO;
    sfm_diag::write(file==File::Updates?"UPDATES_SD":"DRIVE_SD", std::string("arquivo=") + filename(file) +
                    " etapa=" + stage + " errno=" + std::to_string(error.recoveryCode));
}

bool exists(const std::string& path, bool& found, Error& error, File file, const std::string& stage) {
    struct stat info{};
    if (!before(stage)) return fail(error, file, stage, errno);
    if (lstat(path.c_str(), &info) == 0) {
        if (!S_ISREG(info.st_mode)) return fail(error, file, stage, S_ISDIR(info.st_mode) ? EISDIR : EINVAL);
        found = true; return true;
    }
    if (errno == ENOENT) { found = false; return true; }
    return fail(error, file, stage, errno);
}

bool directory(const std::string& path, Error& error) {
    if (!before("criar-diretorio")) return fail(error, File::Token, "criar-diretorio", errno);
    if (mkdir(path.c_str(), 0777) == 0) return true;
    int code = errno;
    if (code == EEXIST) {
        struct stat info{};
        if (stat(path.c_str(), &info) == 0) {
            if (S_ISDIR(info.st_mode)) return true;
            code = ENOTDIR;
        } else code = errno;
    }
    return fail(error, File::Token, "criar-diretorio", code);
}

bool ensure(const std::string& path, Error& error) {
    if (path.empty()) return fail(error, File::Token, "validar-diretorio", EINVAL);
    size_t component = 0;
    while (component < path.size()) {
        size_t end = path.find('/', component);
        if (end == std::string::npos) end = path.size();
        if (path.substr(component, end - component) == "..") return fail(error, File::Token, "validar-diretorio", EINVAL);
        component = end + 1;
    }
    const size_t device = path.find(":/");
    size_t start = device == std::string::npos ? (path.front() == '/' ? 1 : 0) : device + 2;
    for (size_t i = start; i <= path.size(); ++i)
        if ((i == path.size() || path[i] == '/') && i > 0)
            if (!directory(path.substr(0, i), error)) return false;
    return true;
}

std::string pathFor(const std::string& root, File file) { return root + "/" + filename(file); }
std::string slotPath(const std::string& path, const char* label, uint64_t slot) {
    char suffix[64]; snprintf(suffix, sizeof suffix, ".sfm-%s-%03llu", label, static_cast<unsigned long long>(slot));
    return path + suffix;
}

struct Entry { std::string path; uint64_t slot; bool backup; };
bool entries(const std::string& root, File file, std::vector<Entry>& out, Error& error, bool includeLegacyTemps = false) {
    if (!before("listar-transacoes")) return fail(error, file, "listar-transacoes", errno);
    DIR* dir = opendir(root.c_str());
    if (!dir) {
        if (errno == ENOENT) return true;
        return fail(error, file, "listar-transacoes", errno);
    }
    const std::string name = filename(file);
    bool ok = true;
    for (;;) {
        errno = 0;
        dirent* entry = readdir(dir);
        if (!entry) { if (errno) ok = fail(error, file, "listar-transacoes", errno); break; }
        std::string value = entry->d_name;
        bool backup = value.compare(0, name.size() + 9, name + ".sfm-old-") == 0;
        bool pending = value.compare(0, name.size() + 9, name + ".sfm-new-") == 0;
        if (!backup && !pending) {
            if (includeLegacyTemps && (value == name || value.compare(0, name.size() + 5, name + ".tmp.") == 0))
                out.push_back({root + "/" + value, 0, false});
            continue;
        }
        std::string digits = value.substr(name.size() + 9);
        if (digits.size() < 3 || digits.size() > 20 || digits.find_first_not_of("0123456789") != std::string::npos) continue;
        errno = 0;
        unsigned long long slot = strtoull(digits.c_str(), nullptr, 10);
        if (errno == ERANGE) continue;
        out.push_back({root + "/" + value, static_cast<uint64_t>(slot), backup});
    }
    if (closedir(dir) != 0 && ok) ok = fail(error, file, "fechar-diretorio", errno);
    return ok;
}

bool read(const std::string& path, std::string& data, Error& error, File file, const char* phase) {
    const std::string openStage = std::string("abrir-leitura-") + phase;
    const std::string readStage = std::string("ler-") + phase;
    const std::string closeStage = std::string("fechar-leitura-") + phase;
    if (!before(openStage)) return fail(error, file, openStage, errno);
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) return fail(error, file, openStage, errno);
    std::string value;
    char buffer[4096];
    bool ok = true;
    for (;;) {
        ssize_t count = before(readStage) ? ::read(fd, buffer, sizeof buffer) : -1;
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) { fail(error, file, readStage, errno); ok = false; break; }
        if (!count) break;
        if (value.size() + static_cast<size_t>(count) > maxBytes) {
            fail(error, file, readStage, EFBIG); ok = false; break;
        }
        value.append(buffer, static_cast<size_t>(count));
    }
    int injected = before(closeStage) ? 0 : errno;
    int result = close(fd);
    if ((injected || result != 0) && ok) { fail(error, file, closeStage, injected ? injected : errno); ok = false; }
    if (ok) data.swap(value);
    return ok;
}

// fsdev não substitui destinos existentes. Só movemos para nomes livres.
bool move(const std::string& source, const std::string& target, const std::string& stage,
          Error& error, File file) {
    if (!before(stage)) return fail(error, file, stage, errno);
    struct stat info{};
    if (lstat(target.c_str(), &info) == 0) return fail(error, file, stage, EEXIST);
    if (errno != ENOENT) return fail(error, file, stage, errno);
    if (rename(source.c_str(), target.c_str()) != 0) return fail(error, file, stage, errno);
    return true;
}

bool recover(const std::string& path, File file, Error& error) {
    bool current = false;
    if (!exists(path, current, error, file, "examinar-legado")) return false;
    if (current) return true;
    // Apenas backups reservados pela aplicação; nunca interpretar temporários como login válido.
    const std::string root = path.substr(0, path.find_last_of('/'));
    std::vector<Entry> candidates;
    if (!entries(root, file, candidates, error)) return false;
    std::sort(candidates.begin(), candidates.end(), [](const Entry& a, const Entry& b) { return a.slot > b.slot; });
    for (const auto& candidate : candidates) {
        if (!candidate.backup) continue;
        std::string backup = candidate.path;
        bool found = false;
        if (!exists(backup, found, error, file, "examinar-recuperacao")) return false;
        if (!found) continue;
        std::string previous;
        if (!read(backup, previous, error, file, "recuperacao")) return false;
        if (previous.empty()) return fail(error, file, "validar-recuperacao", EINVAL);
        if (!move(backup, path, "restaurar-recuperacao", error, file)) return false;
        std::string confirmed;
        if (!read(path, confirmed, error, file, "recuperado")) return false;
        if (confirmed != previous) return fail(error, file, "comparar-recuperacao", EIO);
        sfm_diag::write(file==File::Updates?"UPDATES_SD":"DRIVE_SD", std::string("arquivo=") + filename(file) + " etapa=recuperado errno=0");
        return true;
    }
    return true;
}

void rollback(const std::string& path, const std::string& temporary, const std::string& backup,
              bool hadPrevious, bool installed, File file, Error& error) {
    Error recovery;
    if (installed && !move(path, temporary, "isolar-novo", recovery, file)) {
        recoveryFailure(error, file, recovery.stage, recovery.code); return;
    }
    if (hadPrevious && !move(backup, path, "restaurar-anterior", recovery, file)) {
        recoveryFailure(error, file, recovery.stage, recovery.code); return;
    }
    sfm_diag::write(file==File::Updates?"UPDATES_SD":"DRIVE_SD", std::string("arquivo=") + filename(file) + " etapa=rollback-concluido errno=0");
}
}

const char* filename(File file) {
    switch (file) {
        case File::Credentials: return "google.json";
        case File::Token: return "token.txt";
        case File::LoginState: return "login.disabled";
        case File::Account: return "drive-account.json";
        case File::Transfer: return "transfer.bin";
        case File::Updates: return "update-settings.json";
    }
    return "desconhecido";
}

std::string describe(const Error& error) {
    std::string text = "etapa=" + error.stage + "; errno=" + std::to_string(error.code);
    if (error.recoveryCode) text += "; recuperacao=" + error.recoveryStage + "; errno=" + std::to_string(error.recoveryCode);
    return text;
}

bool ensureDirectory(const std::string& root, Error& error) {
    std::lock_guard<std::mutex> guard(storeLock); error = {};
    return ensure(root, error);
}

bool load(const std::string& root, File file, std::string& data, Error& error, const char* phase) {
    std::lock_guard<std::mutex> guard(storeLock); error = {};
    const std::string path = pathFor(root, file);
    if (!recover(path, file, error)) return false;
    bool present = false;
    if (!exists(path, present, error, file, "examinar-legado")) return false;
    if (!present) { error.stage = "arquivo-ausente"; error.code = ENOENT; return false; }
    return read(path, data, error, file, phase);
}

bool save(const std::string& root, File file, const std::string& data, Error& error) {
    std::lock_guard<std::mutex> guard(storeLock); error = {};
    if (data.empty() || data.size() > maxBytes) return fail(error, file, "validar-dados", data.empty() ? EINVAL : EFBIG);
    if (!ensure(root, error)) return false;
    const std::string path = pathFor(root, file);
    if (!recover(path, file, error)) return false;
    bool hadPrevious = false;
    if (!exists(path, hadPrevious, error, file, "examinar-legado")) return false;
    std::string previous;
    if (hadPrevious && !read(path, previous, error, file, "anterior")) return false;
    if (hadPrevious && previous == data) return true; // releitura válida, sem desgaste/substituição desnecessária
    std::vector<Entry> occupied;
    if (!entries(root, file, occupied, error)) return false;
    uint64_t next = 0;
    for (const auto& entry : occupied) {
        if (entry.slot == std::numeric_limits<uint64_t>::max()) return fail(error, file, "reservar-transacao", EOVERFLOW);
        next = std::max(next, entry.slot + 1);
    }
    const std::string temporary = slotPath(path, "new", next);
    const std::string backup = slotPath(path, "old", next);
    if (!before("abrir-temporario")) return fail(error, file, "abrir-temporario", errno);
    int fd = open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) return fail(error, file, "abrir-temporario", errno);
    bool ok = true;
    size_t offset = 0;
    while (offset < data.size()) {
        size_t amount = data.size() - offset;
#ifdef HOST_TEST
        if (writeLimit && amount > writeLimit) amount = writeLimit;
#endif
        ssize_t count = before("gravar-temporario") ? ::write(fd, data.data() + offset, amount) : (errno ? -1 : 0);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { fail(error, file, "gravar-temporario", count ? errno : EIO); ok = false; break; }
        offset += static_cast<size_t>(count);
    }
    if (ok) {
        int result;
        do { result = before("sincronizar-temporario") ? fsync(fd) : -1; } while (result < 0 && errno == EINTR);
        if (result != 0) { fail(error, file, "sincronizar-temporario", errno); ok = false; }
    }
    int injected = before("fechar-temporario") ? 0 : errno;
    int closed = close(fd);
    if (ok && (injected || closed != 0)) { fail(error, file, "fechar-temporario", injected ? injected : errno); ok = false; }
    if (!ok) return false; // preservar o anterior e o temporário; nunca remover dados para "destravar"
    std::string confirmed;
    if (!read(temporary, confirmed, error, file, "temporario")) return false;
    if (!before("comparar-temporario") || confirmed != data) return fail(error, file, "comparar-temporario", errno ? errno : EIO);
    if (hadPrevious) {
        std::string current;
        if (!read(path, current, error, file, "antes-substituicao")) return false;
        if (current != previous) return fail(error, file, "origem-alterada", EAGAIN);
        if (!move(path, backup, "mover-anterior", error, file)) return false;
    }
    if (!move(temporary, path, "instalar-novo", error, file)) {
        rollback(path, temporary, backup, hadPrevious, false, file, error); return false;
    }
    confirmed.clear();
    if (!read(path, confirmed, error, file, "final") || !before("comparar-final") || confirmed != data) {
        if (!error.code) fail(error, file, "comparar-final", errno ? errno : EIO);
        rollback(path, temporary, backup, hadPrevious, true, file, error); return false;
    }
    sfm_diag::write(file==File::Updates?"UPDATES_SD":"DRIVE_SD", std::string("arquivo=") + filename(file) + " etapa=gravado-e-relido errno=0");
    return true;
}

bool eraseMatching(const std::string& root, File file,
                   const std::function<EraseDecision(const std::string&)>& decide,
                   unsigned& unknown, Error& error) {
    std::lock_guard<std::mutex> guard(storeLock); error = {};
    std::vector<Entry> files;
    if (!entries(root, file, files, error, true)) return false;
    bool ok = true;
    for (const auto& entry : files) {
        Error operation;
        std::string data;
        bool found = false;
        if (!exists(entry.path, found, operation, file, "examinar-esquecimento") ||
            (found && !read(entry.path, data, operation, file, "esquecimento"))) {
            if (ok) error = operation;
            ok = false; continue;
        }
        if (!found) continue;
        auto decision = decide(data);
        if (decision == EraseDecision::Unknown) { ++unknown; continue; }
        if (decision != EraseDecision::Remove) continue;
        if (!before("apagar-autenticacao") || unlink(entry.path.c_str()) != 0) {
            int code = errno;
            if (ok) fail(error, file, "apagar-autenticacao", code);
            ok = false;
        }
    }
    return ok;
}

#ifdef HOST_TEST
void testFaults(std::vector<Fault> value) { faults = std::move(value); }
void testWriteLimit(unsigned value) { writeLimit = value; }
void testCrashAt(const std::string& value) { crashStage = value; }
#endif
}
