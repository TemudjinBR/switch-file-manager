#include "cloud/download_manager.h"
#include "cloud/credential_store.h"
#include "security/oauth.h"
#include "sfm_diag.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <limits>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <thread>
#include <unistd.h>
#ifdef HOST_TEST
#include <sys/syscall.h>
#endif

namespace gd::downloads {
namespace {
constexpr const char* magic = "SFM_DOWNLOAD_AREA_1\n";
#ifdef HOST_TEST
std::string faultStage, crashStage;
int faultCode = 0;
#endif
bool before(const char* stage) {
#ifdef HOST_TEST
    if (crashStage == stage) std::_Exit(77);
    if (faultStage == stage) { faultStage.clear(); errno = faultCode; return false; }
#else
    (void)stage;
#endif
    return true;
}
bool fail(Error& error, Kind kind, const std::string& stage, int code, const std::string& message) {
    error = {kind, stage, code, message};
    sfm_diag::write("DRIVE_DOWNLOAD", "etapa=" + stage + " codigo=" + std::to_string(code));
    return false;
}
bool sd(Error& error, const char* stage, int code) { return fail(error, Kind::Storage, stage, code ? code : EIO, "Falha no SD; parcial preservado"); }
uint32_t crc(uint32_t previous, const uint8_t* data, size_t length) {
    static const auto table = [] {
        std::array<uint32_t, 256> values{};
        for (unsigned i = 0; i < values.size(); ++i) {
            uint32_t v = i;
            for (unsigned bit = 0; bit < 8; ++bit) v = (v >> 1) ^ (0xedb88320u & (0u - (v & 1)));
            values[i] = v;
        }
        return values;
    }();
    uint32_t value = ~previous;
    for (size_t i = 0; i < length; ++i) value = table[(value ^ data[i]) & 255] ^ (value >> 8);
    return ~value;
}
void number(std::string& out, uint64_t value) { for (unsigned i = 0; i < 8; ++i) out += char(value >> (i * 8)); }
void field(std::string& out, const std::string& value) { number(out, value.size()); out += value; }
std::string encode(const Job& job) {
    std::string out = "SFMDL2\n";
    for (auto value : {job.owner, job.id, job.destination, job.remote.version, job.remote.mediaTag}) field(out, value);
    for (uint64_t value : {job.remote.size, job.offset, job.generation, uint64_t(job.checksum), uint64_t(job.state)}) number(out, value);
    number(out, crc(0, reinterpret_cast<const uint8_t*>(out.data()), out.size()));
    return out;
}
bool decode(const std::string& data, Job& job) {
    if (data.size() < 7 || data.compare(0, 7, "SFMDL2\n") != 0) return false;
    size_t cursor = 7;
    auto readNumber = [&](uint64_t& value) {
        if (data.size() - cursor < 8) return false;
        value = 0;
        for (unsigned i = 0; i < 8; ++i) value |= uint64_t(uint8_t(data[cursor++])) << (i * 8);
        return true;
    };
    for (auto* value : {&job.owner, &job.id, &job.destination, &job.remote.version, &job.remote.mediaTag}) {
        uint64_t size;
        if (!readNumber(size) || size > 4096 || size > data.size() - cursor) return false;
        *value = data.substr(cursor, size); cursor += size;
        if (value->find('\0') != std::string::npos) return false;
    }
    uint64_t checksum, state, hash;
    if (!readNumber(job.remote.size) || !readNumber(job.offset) || !readNumber(job.generation) || !readNumber(checksum) || !readNumber(state)) return false;
    size_t hashed = cursor;
    if (!readNumber(hash) || cursor != data.size() || hash != crc(0, reinterpret_cast<const uint8_t*>(data.data()), hashed)) return false;
    if (state > uint64_t(State::Failed) || checksum > UINT32_MAX || job.offset > job.remote.size || job.remote.size > uint64_t(INT64_MAX)) return false;
    job.checksum = uint32_t(checksum); job.state = State(state); job.remote.id = job.id;
    return !job.id.empty() && !job.owner.empty() && !job.destination.empty();
}
std::string keyFor(const std::string& owner, const std::string& id, const std::string& dest) {
    std::string data; field(data, owner); field(data, id); field(data, dest);
    return sfm::oauth::challenge(data);
}
std::string partial(const std::string& dir, uint64_t generation) { return dir + "/partial-" + std::to_string(generation); }
bool regular(const std::string& path, uint64_t& size, Error& error, bool& present) {
    struct stat info{};
    if (lstat(path.c_str(), &info) != 0) {
        if (errno == ENOENT) { present = false; return true; }
        return sd(error, "examinar-arquivo", errno);
    }
    if (!S_ISREG(info.st_mode) || info.st_size < 0) return sd(error, "tipo-arquivo", EINVAL);
    present = true; size = uint64_t(info.st_size); return true;
}
bool areaCheck(const std::string& root, bool create, Error& error) {
    bool fresh = false;
    struct stat info{};
    if (lstat(root.c_str(), &info) != 0) {
        if (errno != ENOENT) return sd(error, "examinar-area", errno);
        if (!create) return false;
        if (mkdir(root.c_str(), 0700) != 0) return sd(error, "criar-area", errno);
        fresh = true;
    } else if (!S_ISDIR(info.st_mode)) return sd(error, "tipo-area", ENOTDIR);
    const std::string marker = root + "/area.txt";
    if (fresh) {
        int fd = open(marker.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (fd < 0) return sd(error, "criar-marcador", errno);
        size_t size = strlen(magic);
        ssize_t count = write(fd, magic, size);
        int code = count != ssize_t(size) ? (errno ? errno : EIO) : 0;
        if (!code && fsync(fd) != 0) code = errno;
        if (close(fd) != 0 && !code) code = errno;
        if (code) return sd(error, "gravar-marcador", code);
    }
    int fd = open(marker.c_str(), O_RDONLY);
    if (fd < 0) return sd(error, "validar-propriedade-area", errno);
    char value[64]{}; ssize_t count = read(fd, value, sizeof value); int code = errno;
    if (close(fd) != 0) return sd(error, "fechar-marcador", errno);
    if (count != ssize_t(strlen(magic)) || memcmp(value, magic, strlen(magic)) != 0) return sd(error, "validar-propriedade-area", count < 0 ? code : EINVAL);
    return true;
}
bool load(const std::string& dir, Job& job, Error& error) {
    persist::Error disk;
    std::string text;
    if (!persist::load(dir, persist::File::Transfer, text, disk)) return sd(error, disk.stage.c_str(), disk.code);
    if (!decode(text, job)) return sd(error, "validar-registro-download", EINVAL);
    return true;
}
bool save(const std::string& dir, const Job& job, Error& error) {
    if (!before("salvar-checkpoint")) return sd(error, "salvar-checkpoint", errno);
    persist::Error disk;
    if (!persist::save(dir, persist::File::Transfer, encode(job), disk)) return sd(error, disk.stage.c_str(), disk.code);
    return true;
}
bool safeDestination(const std::string& path) {
    if (path.empty() || path.back() == '/' || path.find('\0') != std::string::npos) return false;
    size_t start = 0;
    while (start <= path.size()) {
        size_t end = path.find('/', start); if (end == std::string::npos) end = path.size();
        if (path.substr(start, end - start) == "..") return false;
        start = end + 1;
    }
#ifndef HOST_TEST
    if (path.compare(0, 6, "sdmc:/") != 0) return false;
#endif
    return true;
}
bool same(const Remote& a, const Remote& b) { return a.id == b.id && a.version == b.version && a.size == b.size; }
bool verifyPrefix(int fd, uint64_t length, uint32_t expected, Error& error, Observer* observer = nullptr) {
    if (lseek(fd, 0, SEEK_SET) < 0) return sd(error, "posicionar-verificacao", errno);
    uint8_t buffer[65536]; uint32_t sum = 0;
    uint64_t offset = 0;
    while (offset < length) {
        if (observer) {
            do {
                if (!observer->bytes(0) || observer->action() == Action::Cancel)
                    return fail(error, Kind::Protocol, "cancelado-verificacao", 0, "Cancelado; parcial preservado");
                if (observer->action() != Action::Pause) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            } while (true);
        }
        ssize_t count = read(fd, buffer, size_t(std::min<uint64_t>(sizeof buffer, length - offset)));
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return sd(error, "reler-parcial", count ? errno : EIO);
        sum = crc(sum, buffer, count); offset += uint64_t(count);
    }
    if (sum != expected) return sd(error, "checksum-parcial", EIO);
    return true;
}
bool moveFinal(const std::string& from, const std::string& to, Error& error) {
    if (!before("publicar-destino")) return sd(error, "publicar-destino", errno);
#ifdef HOST_TEST
    // Linux: publica atomicamente sem substituir arquivo concorrente.
    if (syscall(SYS_renameat2, AT_FDCWD, from.c_str(), AT_FDCWD, to.c_str(), 1u) != 0)
#else
    // fsdev/libnx recusa destino existente, ao contrario de rename POSIX.
    if (rename(from.c_str(), to.c_str()) != 0)
#endif
        return sd(error, "publicar-destino", errno);
    return true;
}
struct Descriptor { int value = -1; ~Descriptor() { if (value >= 0) close(value); } };
}
const char* label(State state) {
    switch (state) {
        case State::Connecting: return "Conectando";
        case State::Downloading: return "Baixando";
        case State::Retrying: return "Reconectando";
        case State::Paused: return "Pausado";
        case State::Restarting: return "Reiniciando com seguranca";
        case State::Validating: return "Verificando";
        case State::Finalizing: return "Finalizando";
        case State::Completed: return "Concluido";
        case State::Cancelled: return "Cancelado";
        case State::Failed: return "Interrompido";
    }
    return "Interrompido";
}

bool run(const std::string& area, const std::string& owner, const std::string& id,
         const std::string& destination, Transport& transport, Observer* observer, Error& error, Policy policy) {
    error = {};
    if (owner.empty() || id.empty() || !safeDestination(destination) || !policy.chunkBytes || policy.maxRetries > 20)
        return fail(error, Kind::Protocol, "validar-pedido", EINVAL, "Pedido de download invalido");
    if (destination == area || destination.compare(0, area.size() + 1, area + "/") == 0)
        return fail(error, Kind::Protocol, "destino-reservado", EINVAL, "Area tecnica reservada");
    if (!areaCheck(area, true, error)) return false;
    Job job; job.owner = owner; job.id = id; job.destination = destination;
    job.key = keyFor(owner, id, destination);
    std::string dir = area + "/" + job.key;
    bool fresh = mkdir(dir.c_str(), 0700) == 0;
    if (!fresh && errno != EEXIST) return sd(error, "criar-tarefa", errno);
    if (!fresh) {
        struct stat folder{};
        if (lstat(dir.c_str(), &folder) != 0 || !S_ISDIR(folder.st_mode)) return sd(error, "tipo-tarefa", ENOTDIR);
        if (!load(dir, job, error)) {
            if (error.stage != "arquivo-ausente") return false;
            // Interrupcao antes do primeiro registro: preservar candidatos, iniciar outro parcial.
            DIR* folder = opendir(dir.c_str());
            if (!folder) return false;
            bool recognized = true;
            while (auto* entry = readdir(folder)) {
                std::string name = entry->d_name;
                if (name == "." || name == "..") continue;
                bool known = name.rfind("partial-", 0) == 0 || name.rfind("transfer.bin.sfm-new-", 0) == 0;
                struct stat info{};
                if (!known || lstat((dir + "/" + name).c_str(), &info) != 0 || !S_ISREG(info.st_mode)) { recognized = false; break; }
            }
            closedir(folder);
            if (!recognized) return false;
            fresh = true; error = {};
        }
        if (job.owner != owner || job.id != id || job.destination != destination || keyFor(owner, id, destination) != job.key)
            return fail(error, Kind::Protocol, "vincular-tarefa", EINVAL, "Registro de outra tarefa preservado");
    }
    uint64_t destinationSize = 0; bool destinationPresent = false;
    if (!regular(destination, destinationSize, error, destinationPresent)) return false;
    if (destinationPresent) {
        bool present = false; uint64_t size = 0;
        if (!fresh && job.state == State::Finalizing && regular(partial(dir, job.generation), size, error, present) && !present && destinationSize == job.remote.size) {
            Descriptor final; final.value = open(destination.c_str(), O_RDONLY);
            if (final.value >= 0 && verifyPrefix(final.value, job.offset, job.checksum, error)) {
                job.state = State::Completed; return save(dir, job, error);
            }
        }
        return sd(error, "destino-existente", EEXIST);
    }
    Descriptor file;
    Status status; status.maxAttempts = policy.maxRetries + 1;
    Action action = Action::Continue;
    auto notify = [&](State state, const std::string& reason = "") {
        status.state = state; status.current = job.offset; status.total = job.remote.size; status.reason = reason;
        if (observer) observer->update(status);
    };
    auto poll = [&]() {
        if (observer) {
            if (!observer->bytes(0)) action = Action::Cancel;
            else action = observer->action();
        }
        return action;
    };
    auto sync = [&]() {
        int result;
        do { result = before("sincronizar-parcial") ? fsync(file.value) : -1; } while (result < 0 && errno == EINTR);
        return result == 0 || sd(error, "sincronizar-parcial", errno);
    };
    auto openPartial = [&](bool create) {
        for (unsigned slots = 0; slots < 1024; ++slots) {
            file.value = open(partial(dir, job.generation).c_str(), O_RDWR | (create ? O_CREAT | O_EXCL : 0), 0600);
            if (file.value >= 0 || !create || errno != EEXIST) break;
            if (job.generation == UINT64_MAX) return sd(error, "reservar-parcial", EOVERFLOW);
            ++job.generation;
        }
        if (file.value < 0) return sd(error, "abrir-parcial", errno);
        uint64_t size = 0; bool present = false;
        if (!regular(partial(dir, job.generation), size, error, present) || size < job.offset) return sd(error, "tamanho-parcial", EIO);
        if (!verifyPrefix(file.value, job.offset, job.checksum, error, observer)) return false;
        if (ftruncate(file.value, off_t(job.offset)) != 0) return sd(error, "restaurar-checkpoint", errno);
        return true;
    };
    if (!openPartial(fresh)) return false;
    auto checkpoint = [&](State state) {
        if (file.value >= 0 && !sync()) return false;
        job.state = state;
        return save(dir, job, error);
    };
    auto control = [&]() {
        if (poll() == Action::Cancel) {
            if (!checkpoint(State::Cancelled)) return false;
            notify(State::Cancelled);
            fail(error, Kind::Protocol, "cancelado", 0, "Cancelado; parcial preservado para retomada explicita");
            return false;
        }
        if (action == Action::Pause) {
            if (!checkpoint(State::Paused)) return false;
            if (!before("pausado")) return sd(error, "pausado", errno);
            do {
                notify(State::Paused);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            } while (poll() == Action::Pause);
            if (action == Action::Cancel) {
                if (!checkpoint(State::Cancelled)) return false;
                notify(State::Cancelled);
                fail(error, Kind::Protocol, "cancelado", 0, "Cancelado; parcial preservado");
                return false;
            }
        }
        return true;
    };
    if (fresh && !checkpoint(State::Connecting)) return false;
    unsigned retries = 0;
    auto retry = [&](const std::string& reason) {
        if (retries >= policy.maxRetries) return false;
        ++retries; status.attempt = retries + 1;
        unsigned factor = 1u << std::min(retries - 1, 15u);
        uint64_t delay = std::min<uint64_t>(uint64_t(policy.baseDelayMs) * factor, policy.maxDelayMs);
        for (uint64_t elapsed = 0; elapsed < delay;) {
            status.delayMs = unsigned(delay - elapsed); notify(State::Retrying, reason);
            if (!control()) return false;
            auto slice = std::min<uint64_t>(50, delay - elapsed);
            std::this_thread::sleep_for(std::chrono::milliseconds(slice)); elapsed += slice;
        }
        status.delayMs = 0; return true;
    };
    auto restart = [&](const Remote& remote, const std::string& reason) {
        if (reason != "novo-download" && observer && !observer->restart(reason))
            return fail(error, Kind::Protocol, "reinicio-adiado", 0, "Reinicio adiado; parcial e registro preservados");
        notify(State::Restarting, reason);
        if (file.value >= 0) {
            if (close(file.value) != 0) { file.value = -1; return sd(error, "fechar-parcial-anterior", errno); }
            file.value = -1;
            if (job.generation == UINT64_MAX) return sd(error, "reservar-parcial", EOVERFLOW);
            ++job.generation;
        }
        job.remote = remote; job.remote.mediaTag.clear(); job.offset = 0; job.checksum = 0;
        if (!openPartial(true)) return false;
        return checkpoint(State::Connecting);
    };
    for (;;) {
        if (!control()) return false;
        notify(State::Connecting);
        Remote remote; Error inspection;
        if (!transport.inspect(id, remote, inspection)) {
            error = inspection;
            if (inspection.kind == Kind::Network && retry(inspection.stage)) continue;
            break;
        }
        if (remote.id != id || remote.version.empty() || remote.size > uint64_t(INT64_MAX)) {
            fail(error, Kind::Protocol, "identidade-remota", EINVAL, "Sem identidade/versao remota verificavel"); break;
        }
        error = {};
        if (fresh || !same(job.remote, remote)) {
            if (!fresh && !job.remote.version.empty() && !retry("versao-remota-alterada")) break;
            if (!restart(remote, fresh ? "novo-download" : "versao-remota-alterada")) return false;
            fresh = false;
        }
        if (job.offset == job.remote.size) {
            notify(State::Validating);
            if (!verifyPrefix(file.value, job.offset, job.checksum, error, observer) || !checkpoint(State::Finalizing)) return false;
            notify(State::Finalizing);
            if (!before("antes-publicar")) return sd(error, "antes-publicar", errno);
            if (close(file.value) != 0) { file.value = -1; return sd(error, "fechar-parcial", errno); }
            file.value = -1;
            if (!moveFinal(partial(dir, job.generation), destination, error)) return false;
            if (!before("apos-publicar")) return sd(error, "apos-publicar", errno);
            job.state = State::Completed;
            if (!save(dir, job, error)) return false;
            error = {}; notify(State::Completed); return true;
        }
        // Consulta o volume do parcial; so faltam os bytes ainda nao gravados. Reservar
        // margem para registros/cluster nao garante exclusividade contra outro escritor.
        struct statvfs volume{};
        if (!before("espaco-livre") || statvfs(dir.c_str(), &volume) != 0) return sd(error, "espaco-livre", errno);
        const uint64_t unit = volume.f_frsize ? volume.f_frsize : volume.f_bsize;
        const uint64_t available = unit && uint64_t(volume.f_bavail) > UINT64_MAX / unit
            ? UINT64_MAX : uint64_t(volume.f_bavail) * unit;
        const uint64_t remaining = job.remote.size - job.offset;
        const uint64_t margin = std::max<uint64_t>(unit, 4096) * 8;
        if (remaining > available || margin > available - remaining)
            return sd(error, "espaco-livre", ENOSPC);
        const uint64_t start = job.offset;
        const uint64_t end = start + std::min(policy.chunkBytes, job.remote.size - start) - 1;
        if (lseek(file.value, off_t(start), SEEK_SET) < 0) return sd(error, "posicionar-parcial", errno);
        uint64_t received = 0, lastSaved = 0; uint32_t sum = job.checksum, startChecksum = job.checksum;
        bool storageFailure = false;
        notify(State::Downloading);
        auto writeChunk = [&](const uint8_t* data, size_t size) {
            if (poll() != Action::Continue) return false;
            if (size > job.remote.size - start - received) return fail(error, Kind::Protocol, "limite-corpo", EFBIG, "Corpo maior que o arquivo remoto");
            size_t offset = 0;
            while (offset < size) {
                ssize_t written = before("gravar-parcial") ? write(file.value, data + offset, size - offset) : -1;
                if (written < 0 && errno == EINTR) continue;
                if (written <= 0) { storageFailure = true; return sd(error, "gravar-parcial", written ? errno : EIO); }
                sum = crc(sum, data + offset, size_t(written)); received += uint64_t(written); offset += size_t(written);
            }
            if (received - lastSaved >= 1024u * 1024u) {
                job.offset = start + received; job.checksum = sum;
                if (!checkpoint(State::Downloading)) { storageFailure = true; return false; }
                lastSaved = received;
            }
            status.current = start + received; status.networkBytes += size;
            if (observer) { observer->update(status); if (!observer->bytes(size)) { action = Action::Cancel; return false; } action = observer->action(); }
            return action == Action::Continue;
        };
        Error network;
        Fetch fetched = transport.fetch(job.remote, start, end, writeChunk, network);
        if (storageFailure || (!error.stage.empty() && error.kind == Kind::Storage)) return false;
        if (fetched.headersValid && received) {
            // Checkpoint local vinculado a versao previamente consultada. Antes de voltar a
            // acrescentar dados ou publicar, o topo do loop consulta novamente a versao remota.
            if (!job.remote.mediaTag.empty() && !fetched.mediaTag.empty() && job.remote.mediaTag != fetched.mediaTag) {
                fetched.restart = true;
            } else {
                job.offset = start + received; job.checksum = sum;
                if (job.remote.mediaTag.empty()) job.remote.mediaTag = fetched.mediaTag;
                if (!checkpoint(State::Downloading)) return false;
                if (!before("apos-checkpoint")) return sd(error, "apos-checkpoint", errno);
            }
        } else if (!fetched.headersValid && job.offset != start) {
            job.offset = start; job.checksum = startChecksum;
            if (!checkpoint(State::Downloading)) return false;
        }
        bool paused = action == Action::Pause;
        if (!control()) return false;
        if (paused) {
            if (ftruncate(file.value, off_t(job.offset)) != 0) return sd(error, "restaurar-apos-pausa", errno);
            error = {}; continue;
        }
        if (fetched.restart) {
            if (!retry("servidor-recusou-retomada")) { fail(error, Kind::Protocol, "retomada-recusada", fetched.http, "Servidor recusou retomada; parcial preservado"); break; }
            Remote next; if (!transport.inspect(id, next, error)) break;
            if (!restart(next, "servidor-recusou-retomada")) return false;
            continue;
        }
        if (fetched.complete && job.offset > start) { error = {}; continue; }
        if (ftruncate(file.value, off_t(job.offset)) != 0) return sd(error, "descartar-sufixo-nao-validado", errno);
        if (!network.stage.empty()) error = network;
        else if (error.stage.empty()) fail(error, Kind::Network, "download-interrompido", fetched.http, "Conexao interrompida; parcial preservado");
        if (!fetched.retryable || error.kind != Kind::Network || !retry(error.stage)) break;
    }
    if (file.value >= 0) {
        Error cause = error;
        checkpoint(State::Failed);
        if (error.stage.empty() || error.stage == cause.stage) error = cause;
    }
    notify(State::Failed, error.stage);
    return false;
}

bool list(const std::string& area, const std::string& owner, std::vector<Job>& jobs, unsigned& invalid, Error& error) {
    jobs.clear(); invalid = 0; error = {};
    struct stat info{};
    if (lstat(area.c_str(), &info) != 0 && errno == ENOENT) return true;
    if (!areaCheck(area, false, error)) return false;
    DIR* directory = opendir(area.c_str());
    if (!directory) return sd(error, "listar-downloads", errno);
    bool ok = true;
    for (;;) {
        errno = 0; dirent* entry = readdir(directory);
        if (!entry) { if (errno) ok = sd(error, "listar-downloads", errno); break; }
        std::string key = entry->d_name;
        if (key.size() != 43 || key.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_") != std::string::npos) continue;
        Job job; Error problem;
        if (!load(area + "/" + key, job, problem) || keyFor(job.owner, job.id, job.destination) != key || !safeDestination(job.destination)) { ++invalid; continue; }
        job.key = key;
        if (job.owner == owner && job.state != State::Completed) jobs.push_back(job);
    }
    if (closedir(directory) != 0 && ok) return sd(error, "fechar-lista-downloads", errno);
    return ok;
}
bool cancel(const std::string& area, const std::string& owner, const Job& requested, Error& error) {
    if (!areaCheck(area, false, error)) return false;
    if (requested.key != keyFor(owner, requested.id, requested.destination)) return fail(error, Kind::Protocol, "vincular-cancelamento", EINVAL, "Outra tarefa preservada");
    Job saved;
    std::string dir = area + "/" + requested.key;
    if (!load(dir, saved, error) || saved.owner != owner || saved.id != requested.id || saved.destination != requested.destination)
        return fail(error, Kind::Protocol, "validar-cancelamento", EINVAL, "Outra tarefa preservada");
    saved.state = State::Cancelled;
    return save(dir, saved, error);
}
#ifdef HOST_TEST
void testFailure(const std::string& stage, int code) { faultStage = stage; faultCode = code; }
void testCrash(const std::string& stage) { crashStage = stage; }
#endif
}
