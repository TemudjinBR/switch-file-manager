#include "updates/update_service.h"
#include "cloud/credential_store.h"
#include "sfm_diag.h"
#include "updates/update_http.h"
#include <algorithm>
#include <cerrno>
#include <ctime>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace sfm::updates {
Service::~Service() { stop(); }
Preferences Service::prefs() const {
    std::lock_guard<std::mutex> guard(mutex);
    return preferences;
}
Snapshot Service::status() const {
    std::lock_guard<std::mutex> guard(mutex);
    return current;
}
bool Service::store(std::string &error) {
    gd::persist::Error disk;
    std::string value = "{\"schema\":1,\"channel\":" + quote(preferences.channel) +
                        ",\"automatic\":" + (preferences.automatic ? "true" : "false") +
                        ",\"last_check\":" + std::to_string(preferences.lastCheck) +
                        ",\"pending\":" + quote(preferences.pending) + "}";
    if (!gd::persist::save(officialDirectory, gd::persist::File::Updates, value, disk)) {
        error = "Preferencia nao gravada: " + gd::persist::describe(disk);
        return false;
    }
    return true;
}
void Service::initialize(std::string path, std::string ver, std::string build) {
    executable = std::move(path);
    installed = std::move(ver);
    identity = std::move(build);
    std::string text;
    gd::persist::Error disk;
    if (gd::persist::load(officialDirectory, gd::persist::File::Updates, text, disk)) {
        Json json;
        if (!parseJson(text, json) || json.integer("schema") != 1 ||
            (json.str("channel") != "stable" && json.str("channel") != "experimental") ||
            json.get("automatic").type != Json::Boolean || json.integer("last_check") == UINT64_MAX) {
            current.error = "Preferencias invalidas preservadas; verificacao automatica bloqueada";
            preferences.automatic = false;
            return;
        }
        preferences = {json.str("channel"), json.str("pending"), json.get("automatic").value == "true",
                       json.integer("last_check")};
    } else if (disk.code != ENOENT) {
        current.error = "SD: preferencias inacessiveis; checagem automatica desativada";
        preferences.automatic = false;
    }
}
bool Service::configure(const std::string &channel, bool automatic, std::string &error) {
    std::lock_guard<std::mutex> guard(mutex);
    if (current.busy) {
        error = "Aguarde/cancele a tarefa em curso";
        return false;
    }
    if (channel != "stable" && channel != "experimental") {
        error = "Canal invalido";
        return false;
    }
    Preferences old = preferences;
    preferences.channel = channel;
    preferences.automatic = automatic;
    if (!store(error)) {
        preferences = old;
        return false;
    }
    current = {};
    return true;
}
bool Service::check(bool startup, std::string &error) {
    {
        std::lock_guard<std::mutex> guard(mutex);
        uint64_t now = uint64_t(time(nullptr));
        if (startup && !preferences.automatic)
            return false;
        uint64_t interval = startup ? 86400 : 900;
        if (preferences.lastCheck &&
            (now < preferences.lastCheck || now - preferences.lastCheck < interval)) {
            error = "Aguarde o intervalo de consulta (manual: 15 min; abertura: 24 h)";
            return false;
        }
        if (current.busy) {
            error = "Tarefa em curso";
            return false;
        }
        preferences.lastCheck = now;
        if (!store(error))
            return false;
    }
    return launch(false, error);
}
bool Service::download(std::string &error) { return launch(true, error); }
bool Service::launch(bool download, std::string &error) {
    std::lock_guard<std::mutex> guard(mutex);
    if (current.busy) {
        error = "Tarefa em curso";
        return false;
    }
    if (download &&
        (current.available.version.empty() || compareVersions(current.available.version, installed) <= 0)) {
        error = "Nenhuma versao mais nova selecionada";
        return false;
    }
    if (joinable) {
        pthread_join(worker, nullptr);
        joinable = false;
    }
    cancelled = false;
    current.busy = true;
    current.ready = false;
    current.error.clear();
    current.state = download ? "Preparando download" : "Verificando em segundo plano";
    current.done = 0;
    current.total = 0;
    if (!download)
        current.available = {};
    pthread_attr_t attr;
    int rc = pthread_attr_init(&attr);
    if (!rc) {
        rc = pthread_attr_setstacksize(&attr, 256 * 1024);
        if (!rc)
            rc=pthread_create(&worker,&attr,download?[](void* self)->void*{static_cast<Service*>(self)->work(true);return nullptr;}:[](void* self)->void*{static_cast<Service*>(self)->work(false);return nullptr;},this);
        pthread_attr_destroy(&attr);
    }
    if (rc) {
        current.busy = false;
        current.error = "thread de atualizacao pthread=" + std::to_string(rc);
        error = current.error;
        return false;
    }
    joinable = true;
    return true;
}
void Service::work(bool download) {
    std::string error;
    Snapshot result = status();
    Preferences p = prefs();
    bool ok = false;
    auto read = [&](const std::string &url, uint64_t limit, std::string &text) {
        Response response;
        text.clear();
        return getHttps(
            url, limit,
            [&](const uint8_t *data, size_t n) {
                text.append(reinterpret_cast<const char *>(data), n);
                return !cancelled;
            },
            cancelled, response, error);
    };
    if (!download) {
        std::string text;
        std::string api = "https://api.github.com/repos/TemudjinBR/switch-file-manager/releases" +
                          (p.channel == "stable" ? std::string("/latest") : std::string("?per_page=10"));
        if (read(api, 256 * 1024, text)) {
            Json releases;
            if (!parseJson(text, releases)) {
                error = "Resposta GitHub invalida";
            } else {
                std::vector<Json> rows =
                    releases.type == Json::Array ? releases.array : std::vector<Json>{releases};
                Json chosen;
                std::string tag;
                for (const auto &row : rows) {
                    bool experimental =
                        row.get("prerelease").type == Json::Boolean && row.get("prerelease").value == "true";
                    if (row.get("draft").type != Json::Boolean ||
                        row.get("prerelease").type != Json::Boolean || row.get("draft").value != "false" ||
                        experimental != (p.channel == "experimental"))
                        continue;
                    std::string candidate = row.str("tag_name");
                    if (candidate.empty() || candidate.front() != 'v' || !version(candidate.substr(1)))
                        continue;
                    if (tag.empty() || compareVersions(candidate.substr(1), tag.substr(1)) > 0) {
                        chosen = row;
                        tag = candidate;
                    }
                }
                if (tag.empty() || compareVersions(tag.substr(1), installed) <= 0) {
                    ok = true;
                    result.state = "Nenhuma atualizacao mais nova neste canal";
                    result.available = {};
                } else {
                    std::string manifestUrl;
                    for (const auto &asset : chosen.get("assets").array)
                        if (asset.str("name") == "update.json" &&
                            officialUrl(asset.str("browser_download_url"), tag, "update.json"))
                            manifestUrl = asset.str("browser_download_url");
                    if (manifestUrl.empty()) {
                        ok = true;
                        result.state = "Release sem manifesto compativel; consulte instalacao Android";
                        result.available = {};
                    } else if (read(manifestUrl, 32768, text) &&
                               parseManifest(text, p.channel, result.available, error) &&
                               result.available.tag == tag &&
                               chosen.str("target_commitish") == result.available.commit) {
                        bool app = false, helper = false;
                        for (const auto &asset : chosen.get("assets").array) {
                            if (asset.str("browser_download_url") == result.available.app.url &&
                                asset.integer("size") == result.available.app.size)
                                app = true;
                            if (asset.str("browser_download_url") == result.available.helper.url &&
                                asset.integer("size") == result.available.helper.size)
                                helper = true;
                        }
                        if (app && helper) {
                            ok = true;
                            result.state = "Atualizacao disponivel";
                        } else
                            error = "Release/manifesto divergem em assets/tamanhos";
                    } else if (error.empty())
                        error = "Tag do manifesto diverge da Release";
                }
            }
        }
    } else {
        Transaction transaction;
        transaction.manifest = result.available;
        if (runningTarget(executable, installed, identity, error) &&
            hashFile(officialTarget, transaction.oldHash, transaction.oldSize, error) &&
            space(officialDirectory, transaction.manifest.app.size + transaction.manifest.helper.size,
                  error) &&
            reserve(transaction, officialDirectory, error)) {
            const auto fetchFile = [&](const Asset &asset, const std::string &name) {
                int fd =
                    open((transaction.directory + "/" + name).c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
                if (fd < 0) {
                    error = "Criar parcial SD errno=" + std::to_string(errno);
                    return false;
                }
                Response response;
                bool stored = getHttps(
                    asset.url, asset.size,
                    [&](const uint8_t *data, size_t size) {
                        size_t off = 0;
                        while (off < size) {
                            ssize_t count = write(fd, data + off, size - off);
                            if (count < 0 && errno == EINTR)
                                continue;
                            if (count <= 0) {
                                error = "Gravar SD errno=" + std::to_string(errno);
                                return false;
                            }
                            off += size_t(count);
                        }
                        std::lock_guard<std::mutex> guard(mutex);
                        current.done += size;
                        return !cancelled;
                    },
                    cancelled, response, error);
                if (fsync(fd) != 0) {
                    stored = false;
                    error = "Sincronizar parcial SD errno=" + std::to_string(errno);
                }
                if (close(fd) != 0) {
                    stored = false;
                    error = "Fechar parcial SD errno=" + std::to_string(errno);
                }
                return stored;
            };
            {
                std::lock_guard<std::mutex> guard(mutex);
                current.state = "Baixando; use Cancelar tarefa";
                current.total = transaction.manifest.app.size + transaction.manifest.helper.size;
            }
            if (fetchFile(transaction.manifest.app, "new.nro") &&
                fetchFile(transaction.manifest.helper, "helper.nro") && !cancelled &&
                runningTarget(executable, installed, identity, error) &&
                prepareTransaction(transaction, error) && !cancelled) {
                std::lock_guard<std::mutex> guard(mutex);
                preferences.pending = transaction.directory;
                if (store(error)) {
                    ok = true;
                    result.ready = true;
                    result.state = "Verificado; pronto para instalar pelo auxiliar";
                }
            }
        }
    }
    {
        std::lock_guard<std::mutex> guard(mutex);
        result.done = current.done;
        result.total = current.total;
        result.busy = false;
        result.error = ok ? "" : (cancelled ? "Cancelado; NRO instalado e parciais preservados" : error);
        if (!ok)
            result.state = "Interrompido";
        current = std::move(result);
    }
    sfm_diag::write("UPDATES", ok ? "Tarefa concluida" : "Tarefa interrompida; consulte menu Atualizacoes");
}
void Service::cancel() { cancelled = true; }
void Service::stop() {
    cancel();
    if (joinable) {
        pthread_join(worker, nullptr);
        joinable = false;
    }
}
bool Service::pending(Transaction &transaction, std::string &error) const {
    std::string directory;
    {
        std::lock_guard<std::mutex> guard(mutex);
        if (current.busy) {
            error = "Aguarde a tarefa em curso";
            return false;
        }
        directory = preferences.pending;
    }
    if (directory.empty()) {
        error = "Nenhuma transacao registrada";
        return false;
    }
    return readTransaction(directory, transaction, error);
}
} // namespace sfm::updates
