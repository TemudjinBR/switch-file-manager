#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace sfm::updates {
constexpr const char *officialDirectory = "sdmc:/switch/Switch File Manager";
constexpr const char *officialTarget = "sdmc:/switch/Switch File Manager/SwitchFileManager.nro";
constexpr uint64_t maximumNro = 32u << 20;
struct Json {
    enum Type { Null, Object, Array, String, Number, Boolean } type = Null;
    std::map<std::string, Json> object;
    std::vector<Json> array;
    std::string value;
    const Json &get(const std::string &name) const;
    std::string str(const std::string &name) const;
    uint64_t integer(const std::string &name) const;
};
bool parseJson(const std::string &text, Json &result);
std::string quote(const std::string &value);
bool version(const std::string &text);
int compareVersions(const std::string &left, const std::string &right);
struct Asset {
    std::string url, sha256;
    uint64_t size = 0;
};
struct Manifest {
    std::string channel, version, build, commit, notes, tag;
    Asset app, helper;
};
bool parseManifest(const std::string &text, const std::string &channel, Manifest &manifest,
                   std::string &error);
std::string manifestText(const Manifest &manifest);
bool officialUrl(const std::string &url, const std::string &tag, const std::string &name);
bool hashFile(const std::string &path, std::string &hash, uint64_t &size, std::string &error);
bool validateNro(const std::string &path, const Manifest &manifest, bool helper, std::string &error);
bool runningTarget(const std::string &path, const std::string &currentVersion, const std::string &build,
                   std::string &error);
struct Transaction {
    Manifest manifest;
    std::string directory, oldHash;
    uint64_t oldSize = 0;
};
bool reserve(Transaction &transaction, const std::string &directory, std::string &error);
bool writeExclusive(const std::string &path, const std::string &value, std::string &error);
bool readSmall(const std::string &path, std::string &text, std::string &error);
bool space(const std::string &directory, uint64_t bytes, std::string &error);
bool prepareTransaction(Transaction &transaction, std::string &error);
bool readTransaction(const std::string &directory, Transaction &transaction, std::string &error);
// Somente o auxiliar, depois que o aplicativo anterior saiu. Nunca sobrescreve nomes existentes.
bool install(const Transaction &transaction, bool rollback, std::string &error);
#ifdef HOST_TEST
void testFault(const std::string &stage, int code, bool crash = false);
#endif
} // namespace sfm::updates
