#include "updates/update_core.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

namespace sfm::updates {
namespace {
bool fail(std::string &error, const std::string &stage, int code = 0) {
    error = stage + (code ? ": errno=" + std::to_string(code) : "");
    return false;
}
#ifdef HOST_TEST
std::string fault;
int faultCode = 0;
bool crashFault = false;
#endif
bool step(const char *name, std::string &error) {
#ifdef HOST_TEST
    if (fault == name) {
        fault.clear();
        if (crashFault)
            std::_Exit(77);
        return fail(error, name, faultCode);
    }
#else
    (void)name;
    (void)error;
#endif
    return true;
}
bool hex(const std::string &value, size_t size) {
    return value.size() == size && value.find_first_not_of("0123456789abcdef") == std::string::npos;
}
struct Parser {
    const std::string &text;
    size_t p = 0;
    void ws() {
        while (p < text.size() && (text[p] == ' ' || text[p] == '\n' || text[p] == '\r' || text[p] == '\t'))
            ++p;
    }
    bool take(char c) {
        ws();
        if (p < text.size() && text[p] == c) {
            ++p;
            return true;
        }
        return false;
    }
    int code() {
        int result = 0;
        for (int i = 0; i < 4; ++i) {
            if (p >= text.size())
                return -1;
            char c = text[p++];
            int v = c >= '0' && c <= '9'   ? c - '0'
                    : c >= 'a' && c <= 'f' ? c - 'a' + 10
                    : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                           : -1;
            if (v < 0)
                return -1;
            result = (result << 4) | v;
        }
        return result;
    }
    void utf8(std::string &out, unsigned c) {
        if (c < 128)
            out += char(c);
        else if (c < 2048) {
            out += char(0xc0 | (c >> 6));
            out += char(0x80 | (c & 63));
        } else if (c < 65536) {
            out += char(0xe0 | (c >> 12));
            out += char(0x80 | ((c >> 6) & 63));
            out += char(0x80 | (c & 63));
        } else {
            out += char(0xf0 | (c >> 18));
            out += char(0x80 | ((c >> 12) & 63));
            out += char(0x80 | ((c >> 6) & 63));
            out += char(0x80 | (c & 63));
        }
    }
    bool string(std::string &out) {
        if (!take('"'))
            return false;
        out.clear();
        while (p < text.size()) {
            unsigned char c = text[p++];
            if (c == '"')
                return true;
            if (c < 32)
                return false;
            if (c != '\\') {
                out += char(c);
                continue;
            }
            if (p == text.size())
                return false;
            c = text[p++];
            if (c == '"' || c == '\\' || c == '/')
                out += char(c);
            else if (c == 'n')
                out += '\n';
            else if (c == 'r')
                out += '\r';
            else if (c == 't')
                out += '\t';
            else if (c == 'b')
                out += '\b';
            else if (c == 'f')
                out += '\f';
            else if (c == 'u') {
                int v = code();
                if (v < 0 || v == 0)
                    return false;
                if (v >= 0xd800 && v <= 0xdbff) {
                    if (p + 2 > text.size() || text.substr(p, 2) != "\\u")
                        return false;
                    p += 2;
                    int low = code();
                    if (low < 0xdc00 || low > 0xdfff)
                        return false;
                    v = 0x10000 + ((v - 0xd800) << 10) + (low - 0xdc00);
                } else if (v >= 0xdc00 && v <= 0xdfff)
                    return false;
                utf8(out, v);
            } else
                return false;
        }
        return false;
    }
    bool parse(Json &out, unsigned depth = 0) {
        if (depth > 32)
            return false;
        ws();
        if (p == text.size())
            return false;
        if (text[p] == '{') {
            ++p;
            out.type = Json::Object;
            if (take('}'))
                return true;
            do {
                std::string key;
                if (!string(key) || !take(':') || out.object.count(key))
                    return false;
                Json value;
                if (!parse(value, depth + 1))
                    return false;
                out.object.emplace(key, std::move(value));
                if (take('}'))
                    return true;
            } while (take(','));
            return false;
        }
        if (text[p] == '[') {
            ++p;
            out.type = Json::Array;
            if (take(']'))
                return true;
            do {
                Json value;
                if (!parse(value, depth + 1))
                    return false;
                out.array.push_back(std::move(value));
                if (take(']'))
                    return true;
            } while (take(','));
            return false;
        }
        if (text[p] == '"') {
            out.type = Json::String;
            return string(out.value);
        }
        for (auto literal : {"true", "false", "null"})
            if (text.compare(p, strlen(literal), literal) == 0) {
                p += strlen(literal);
                out.type = literal[0] == 'n' ? Json::Null : Json::Boolean;
                out.value = literal;
                return true;
            }
        size_t start = p;
        if (text[p] == '-')
            ++p;
        if (p == text.size() || text[p] < '0' || text[p] > '9')
            return false;
        if (text[p] == '0')
            ++p;
        else
            while (p < text.size() && text[p] >= '0' && text[p] <= '9')
                ++p;
        if (p < text.size() && text[p] == '.') {
            ++p;
            size_t digits = p;
            while (p < text.size() && text[p] >= '0' && text[p] <= '9')
                ++p;
            if (digits == p)
                return false;
        }
        if (p < text.size() && (text[p] == 'e' || text[p] == 'E')) {
            ++p;
            if (p < text.size() && (text[p] == '+' || text[p] == '-'))
                ++p;
            size_t digits = p;
            while (p < text.size() && text[p] >= '0' && text[p] <= '9')
                ++p;
            if (p == digits)
                return false;
        }
        out.type = Json::Number;
        out.value = text.substr(start, p - start);
        return true;
    }
};
// SHA-256 incremental; vetores e arquivos conferidos com hashlib nos testes.
struct Sha {
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    uint8_t buffer[64]{};
    uint64_t count = 0;
    size_t used = 0;
    static uint32_t rot(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }
    void block(const uint8_t *data) {
        constexpr uint32_t k[64] = {

            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        uint32_t w[64];
        for (unsigned i = 0; i < 16; ++i)
            w[i] = uint32_t(data[4 * i]) << 24 | uint32_t(data[4 * i + 1]) << 16 |
                   uint32_t(data[4 * i + 2]) << 8 | data[4 * i + 3];
        for (unsigned i = 16; i < 64; ++i)
            w[i] = w[i - 16] + (rot(w[i - 15], 7) ^ rot(w[i - 15], 18) ^ (w[i - 15] >> 3)) + w[i - 7] +
                   (rot(w[i - 2], 17) ^ rot(w[i - 2], 19) ^ (w[i - 2] >> 10));
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], j = h[7];
        for (unsigned i = 0; i < 64; ++i) {
            uint32_t t = j + (rot(e, 6) ^ rot(e, 11) ^ rot(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
            uint32_t u = (rot(a, 2) ^ rot(a, 13) ^ rot(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
            j = g;
            g = f;
            f = e;
            e = d + t;
            d = c;
            c = b;
            b = a;
            a = t + u;
        }
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
        h[5] += f;
        h[6] += g;
        h[7] += j;
    }
    void update(const uint8_t *data, size_t size) {
        count += size;
        while (size) {
            size_t n = std::min(size, 64 - used);
            memcpy(buffer + used, data, n);
            used += n;
            data += n;
            size -= n;
            if (used == 64) {
                block(buffer);
                used = 0;
            }
        }
    }
    std::string finish() {
        uint64_t bits = count * 8;
        uint8_t pad[128]{};
        pad[0] = 128;
        size_t n = used < 56 ? 56 - used : 120 - used;
        update(pad, n);
        for (unsigned i = 0; i < 8; ++i)
            pad[i] = uint8_t(bits >> (56 - 8 * i));
        update(pad, 8);
        char out[65];
        for (unsigned i = 0; i < 8; ++i)
            snprintf(out + 8 * i, 9, "%08x", unsigned(h[i]));
        return out;
    }
};
bool fileInfo(const std::string &path, uint64_t &size) {
    struct stat st {};
    if (lstat(path.c_str(), &st) != 0)
        return false;
    size = st.st_size;
    return S_ISREG(st.st_mode) && st.st_size >= 0;
}
bool matches(const std::string &path, const std::string &hash, uint64_t size) {
    std::string got, error;
    uint64_t count = 0;
    return hashFile(path, got, count, error) && count == size && got == hash;
}
uint32_t u32(const uint8_t *p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
uint64_t u64(const uint8_t *p) { return uint64_t(u32(p)) | uint64_t(u32(p + 4)) << 32; }
bool safeDirectory(const std::string &directory) {
    std::string prefix = std::string(officialDirectory) + "/.sfm-update-";
    return directory.compare(0, prefix.size(), prefix) == 0 && directory.size() > prefix.size() &&
           directory.find_first_not_of("0123456789", prefix.size()) == std::string::npos;
}
} // namespace
const Json &Json::get(const std::string &key) const {
    static const Json empty;
    auto i = object.find(key);
    return i == object.end() ? empty : i->second;
}
std::string Json::str(const std::string &key) const {
    const auto &v = get(key);
    return v.type == String ? v.value : "";
}
uint64_t Json::integer(const std::string &key) const {
    const auto &v = get(key);
    if (v.type != Number || v.value.empty() || v.value.find_first_not_of("0123456789") != std::string::npos)
        return UINT64_MAX;
    uint64_t n = 0;
    for (char c : v.value) {
        if (n > (UINT64_MAX - unsigned(c - '0')) / 10)
            return UINT64_MAX;
        n = n * 10 + c - '0';
    }
    return n;
}
bool parseJson(const std::string &text, Json &result) {
    result = {};
    if (text.size() > 256 * 1024)
        return false;
    Parser p{text};
    if (!p.parse(result))
        return false;
    p.ws();
    return p.p == text.size();
}
std::string quote(const std::string &value) {
    std::string out = "\"";
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += char(c);
        } else if (c < 32) {
            char b[7];
            snprintf(b, sizeof b, "\\u%04x", c);
            out += b;
        } else
            out += char(c);
    }
    return out + '"';
}
bool version(const std::string &text) {
    if (text.empty() || text.size() > 15)
        return false;
    unsigned parts = 0;
    size_t p = 0;
    while (p < text.size()) {
        size_t end = text.find('.', p);
        if (end == std::string::npos)
            end = text.size();
        if (end == p || (end - p > 1 && text[p] == '0'))
            return false;
        for (size_t i = p; i < end; ++i)
            if (text[i] < '0' || text[i] > '9')
                return false;
        ++parts;
        p = end + 1;
        if (end + 1 == text.size())
            return false;
    }
    return parts == 3;
}
int compareVersions(const std::string &a, const std::string &b) {
    if (!version(a) || !version(b))
        return 0;
    size_t x = 0, y = 0;
    for (int i = 0; i < 3; ++i) {
        uint64_t left = std::strtoull(a.c_str() + x, nullptr, 10),
                 right = std::strtoull(b.c_str() + y, nullptr, 10);
        if (left != right)
            return left < right ? -1 : 1;
        x = a.find('.', x) + 1;
        y = b.find('.', y) + 1;
    }
    return 0;
}
bool officialUrl(const std::string &url, const std::string &tag, const std::string &name) {
    return url == "https://github.com/TemudjinBR/switch-file-manager/releases/download/" + tag + "/" + name;
}
bool parseManifest(const std::string &text, const std::string &channel, Manifest &m, std::string &error) {
    Json j;
    if (!parseJson(text, j) || j.type != Json::Object || j.integer("schema") != 1)
        return fail(error, "Manifesto invalido/incompativel");
    m = {};
    m.channel = j.str("channel");
    m.version = j.str("version");
    m.build = j.str("build");
    m.commit = j.str("commit");
    m.tag = j.str("tag");
    m.notes = j.str("notes");
    for (auto pair : {std::make_pair(&m.app, "app"), std::make_pair(&m.helper, "helper")}) {
        const Json v = j.get(pair.second);
        pair.first->url = v.str("url");
        pair.first->sha256 = v.str("sha256");
        pair.first->size = v.integer("size");
    }
    if ((channel != "stable" && channel != "experimental") || m.channel != channel || !version(m.version) ||
        m.tag != "v" + m.version || !hex(m.commit, 40) || m.commit == std::string(40, '0') ||
        m.notes.size() > 4000 || j.integer("min_updater") != 1)
        return fail(error, "Canal/versao/protocolo do manifesto incompatível");
    const std::string prefix = "v" + m.version + "-";
    if (m.build.compare(0, prefix.size(), prefix) != 0 || !hex(m.build.substr(prefix.size()), 12))
        return fail(error, "Build inconsistente");
    if (!officialUrl(m.app.url, m.tag, "SwitchFileManager.nro") ||
        !officialUrl(m.helper.url, m.tag, "SfmUpdateHelper.nro"))
        return fail(error, "Origem de download nao oficial");
    for (const auto &asset : {m.app, m.helper})
        if (!hex(asset.sha256, 64) || asset.size < 0x4000 || asset.size > maximumNro)
            return fail(error, "Metadados de tamanho/hash invalidos");
    error.clear();
    return true;
}
std::string manifestText(const Manifest &m) {
    auto asset = [](const Asset &a) {
        return "{\"url\":" + quote(a.url) + ",\"size\":" + std::to_string(a.size) +
               ",\"sha256\":" + quote(a.sha256) + "}";
    };
    return "{\"schema\":1,\"min_updater\":1,\"channel\":" + quote(m.channel) +
           ",\"version\":" + quote(m.version) + ",\"build\":" + quote(m.build) + ",\"tag\":" + quote(m.tag) +
           ",\"commit\":" + quote(m.commit) + ",\"notes\":" + quote(m.notes) + ",\"app\":" + asset(m.app) +
           ",\"helper\":" + asset(m.helper) + "}";
}
bool hashFile(const std::string &path, std::string &out, uint64_t &size, std::string &error) {
    uint64_t expected = 0;
    if (!fileInfo(path, expected) || expected > maximumNro)
        return fail(error, "Arquivo/hash inacessivel", errno);
    FILE *f = fopen(path.c_str(), "rb");
    if (!f)
        return fail(error, "Abrir hash", errno);
    Sha sha;
    uint8_t b[32768];
    size = 0;
    while (size <= maximumNro) {
        size_t n = fread(b, 1, sizeof b, f);
        if (!n)
            break;
        sha.update(b, n);
        size += n;
    }
    bool ok = !ferror(f) && size == expected;
    int code = errno;
    if (fclose(f) != 0) {
        ok = false;
        code = errno;
    }
    if (!ok)
        return fail(error, "Ler hash", code);
    out = sha.finish();
    return true;
}
bool validateNro(const std::string &path, const Manifest &m, bool helper, std::string &error) {
    const Asset &asset = helper ? m.helper : m.app;
    uint64_t count = 0;
    std::string hash;
    if (!hashFile(path, hash, count, error) || hash != asset.sha256 || count != asset.size)
        return fail(error, "SHA-256/tamanho divergem; arquivo preservado");
    FILE *f = fopen(path.c_str(), "rb");
    if (!f)
        return fail(error, "Abrir NRO", errno);
    auto read = [&](uint64_t offset, void *dst, size_t n) {
        return offset <= count && n <= count - offset && fseeko(f, off_t(offset), SEEK_SET) == 0 &&
               fread(dst, 1, n, f) == n;
    };
    uint8_t head[0x80]{}, aset[0x38]{};
    bool ok = read(0, head, sizeof head) && memcmp(head + 0x10, "NRO0", 4) == 0;
    uint64_t exe = u32(head + 0x18), prev = 0;
    for (unsigned off : {0x20, 0x28, 0x30}) {
        uint64_t start = u32(head + off), len = u32(head + off + 4);
        ok = ok && len && start >= prev && start + len <= exe;
        prev = start + len;
    }
    ok = ok && exe >= 0x80 && read(exe, aset, sizeof aset) && memcmp(aset, "ASET", 4) == 0;
    // Validar limites antes de somar offsets de 64 bits recebidos do arquivo.
    std::array<std::pair<uint64_t, uint64_t>, 3> assets{};
    unsigned used = 0;
    for (unsigned at : {8u, 24u, 40u}) {
        uint64_t start = u64(aset + at), size = u64(aset + at + 8);
        if (!size)
            continue;
        if (exe > count || start < 0x38 || start > count - exe || size > count - exe - start) {
            ok = false;
            break;
        }
        assets[used++] = {start, start + size};
    }
    // So existem tres assets; conferir pares evita ordenar offsets nao confiaveis.
    for (size_t i = 0; i < assets.size(); ++i)
        for (size_t j = i + 1; j < assets.size(); ++j)
            if (assets[i].first < assets[j].second && assets[j].first < assets[i].second)
                ok = false;
    uint8_t jpeg[2]{};
    ok = ok && u64(aset + 16) > 2 && read(exe + u64(aset + 8), jpeg, sizeof jpeg) && jpeg[0] == 0xff &&
         jpeg[1] == 0xd8;
    uint64_t nacp = u64(aset + 24), length = u64(aset + 32);
    char title[0x200]{}, ver[16]{};
    ok = ok && length == 0x4000 && nacp >= 0x38 && read(exe + nacp, title, sizeof title) &&
         memchr(title, 0, sizeof title) &&
         strcmp(title, helper ? "Atualizador do Switch File Manager" : "Switch File Manager") == 0 &&
         read(exe + nacp + 0x3060, ver, sizeof ver) && memchr(ver, 0, sizeof ver) && m.version == ver;
    const std::string marker = "SFM_BUILD:" + m.build + std::string(1, '\0');
    std::string tail;
    bool found = false;
    for (uint64_t off = 0; ok && off < exe;) {
        char b[8192];
        size_t n = size_t(std::min<uint64_t>(sizeof b, exe - off));
        if (!read(off, b, n)) {
            ok = false;
            break;
        }
        std::string text = tail + std::string(b, n);
        if (text.find(marker) != std::string::npos)
            found = true;
        tail = text.substr(text.size() > 100 ? text.size() - 100 : 0);
        off += n;
    }
    fclose(f);
    return ok && found ? true : fail(error, "NRO/NACP/build inconsistentes");
}
bool runningTarget(const std::string &path, const std::string &current, const std::string &build,
                   std::string &error) {
    if (path != officialTarget)
        return fail(error, "Instalacao bloqueada: caminho em execucao desconhecido ou diferente do oficial");
    Manifest m;
    m.version = current;
    m.build = build;
    uint64_t size = 0;
    if (!hashFile(path, m.app.sha256, size, error))
        return false;
    m.app.size = size;
    return validateNro(path, m, false, error);
}
bool readSmall(const std::string &path, std::string &text, std::string &error) {
    uint64_t size = 0;
    if (!fileInfo(path, size) || size > 32768)
        return fail(error, "Registro inacessivel/invalido", errno);
    FILE *f = fopen(path.c_str(), "rb");
    if (!f)
        return fail(error, "Abrir registro", errno);
    text.resize(size);
    bool ok = fread(text.data(), 1, size, f) == size && !ferror(f);
    if (fclose(f) != 0)
        ok = false;
    return ok ? true : fail(error, "Ler registro", errno);
}
bool writeExclusive(const std::string &path, const std::string &value, std::string &error) {
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0)
        return fail(error, "Criar registro exclusivo", errno);
    size_t pos = 0;
    bool ok = true;
    while (pos < value.size()) {
        ssize_t n = write(fd, value.data() + pos, value.size() - pos);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0) {
            ok = false;
            break;
        }
        pos += size_t(n);
    }
    int code = errno;
    if (ok && fsync(fd) != 0) {
        ok = false;
        code = errno;
    }
    if (close(fd) != 0) {
        ok = false;
        code = errno;
    }
    return ok ? true : fail(error, "Gravar/sincronizar registro", code);
}
bool space(const std::string &dir, uint64_t bytes, std::string &error) {
    if (!step("espaco-livre", error))
        return false;
    struct statvfs v {};
    if (statvfs(dir.c_str(), &v) != 0)
        return fail(error, "Consultar espaco SD", errno);
    uint64_t unit = v.f_frsize ? v.f_frsize : v.f_bsize;
    uint64_t free =
        unit && uint64_t(v.f_bavail) > UINT64_MAX / unit ? UINT64_MAX : uint64_t(v.f_bavail) * unit;
    return bytes <= free && free - bytes >= (1u << 20)
               ? true
               : fail(error, "Espaco insuficiente; falta margem de 1 MiB", ENOSPC);
}
bool reserve(Transaction &t, const std::string &base, std::string &error) {
    if (base != officialDirectory)
        return fail(error, "Diretorio de atualizacao incorreto");
    for (uint64_t i = 0; i < 100000; ++i) {
        std::string dir = base + "/.sfm-update-" + std::to_string(i);
        if (mkdir(dir.c_str(), 0700) == 0) {
            t.directory = dir;
            return writeExclusive(dir + "/owner.txt", "SFM_UPDATE_1\n", error);
        }
        if (errno != EEXIST)
            return fail(error, "Reservar area SD", errno);
    }
    return fail(error, "Limite de areas preservadas");
}
bool prepareTransaction(Transaction &t, std::string &error) {
    if (!safeDirectory(t.directory))
        return fail(error, "Area invalida");
    if (!validateNro(t.directory + "/new.nro", t.manifest, false, error) ||
        !validateNro(t.directory + "/helper.nro", t.manifest, true, error))
        return false;
    if (t.oldHash.empty()) {
        if (!hashFile(officialTarget, t.oldHash, t.oldSize, error))
            return false;
    } else if (!matches(officialTarget, t.oldHash, t.oldSize))
        return fail(error, "NRO alvo mudou durante o download");
    std::string record = "{\"schema\":1,\"old_sha256\":" + quote(t.oldHash) +
                         ",\"old_size\":" + std::to_string(t.oldSize) +
                         ",\"manifest\":" + manifestText(t.manifest) + "}";
    if (!writeExclusive(t.directory + "/transaction.json", record, error))
        return false;
    Transaction loaded;
    return readTransaction(t.directory, loaded, error) && loaded.oldHash == t.oldHash &&
           loaded.oldSize == t.oldSize && manifestText(loaded.manifest) == manifestText(t.manifest);
}
bool readTransaction(const std::string &dir, Transaction &t, std::string &error) {
    std::string marker, text;
    if (!safeDirectory(dir) || !readSmall(dir + "/owner.txt", marker, error) || marker != "SFM_UPDATE_1\n" ||
        !readSmall(dir + "/transaction.json", text, error))
        return fail(error, "Area/journal de atualizacao invalido");
    Json j;
    if (!parseJson(text, j) || j.integer("schema") != 1)
        return fail(error, "Journal invalido");
    // Extrair o objeto manifesto conservando a mesma validacao estrita do caminho normal.
    const auto &v = j.get("manifest");
    Manifest m;
    m.channel = v.str("channel");
    m.version = v.str("version");
    m.build = v.str("build");
    m.tag = v.str("tag");
    m.commit = v.str("commit");
    m.notes = v.str("notes");
    for (auto pair : {std::make_pair(&m.app, "app"), std::make_pair(&m.helper, "helper")}) {
        const Json a = v.get(pair.second);
        *pair.first = {a.str("url"), a.str("sha256"), a.integer("size")};
    }
    Manifest parsed;
    if (v.integer("schema") != 1 || v.integer("min_updater") != 1 ||
        !parseManifest(manifestText(m), m.channel, parsed, error))
        return false;
    t = {parsed, dir, j.str("old_sha256"), j.integer("old_size")};
    return hex(t.oldHash, 64) && t.oldSize >= 0x4000 && t.oldSize <= maximumNro
               ? true
               : fail(error, "Backup/journal inconsistente");
}
bool install(const Transaction &t, bool rollback, std::string &error) {
    Transaction journal;
    if (!readTransaction(t.directory, journal, error) || journal.oldHash != t.oldHash ||
        journal.oldSize != t.oldSize || manifestText(journal.manifest) != manifestText(t.manifest))
        return fail(error, "Transacao alterada");
    const std::string target = officialTarget, backup = t.directory + "/old.nro",
                      next = t.directory + "/new.nro", reverted = t.directory + "/reverted.nro";
    bool currentOld = matches(target, t.oldHash, t.oldSize),
         currentNew = matches(target, t.manifest.app.sha256, t.manifest.app.size);
    bool backupOld = matches(backup, t.oldHash, t.oldSize);
    struct stat st {};
    if (rollback) {
        if (currentOld)
            return true;
        if (!backupOld)
            return fail(error, "Backup nao verificavel; rollback bloqueado");
        // Se o alvo ficou ausente apos interrupcao, tentar restaurar por rename
        // mesmo sem margem para novos dados. O backup nunca e apagado em falha.
        bool missing = lstat(target.c_str(), &st) != 0 && errno == ENOENT;
        if (!missing && !space(t.directory, 0, error))
            return false;
        if (lstat(target.c_str(), &st) == 0) {
            if (!currentNew || lstat(reverted.c_str(), &st) == 0)
                return fail(error, "Destino alterado; nenhum arquivo substituido");
            if (rename(target.c_str(), reverted.c_str()) != 0)
                return fail(error, "Preservar NRO novo", errno);
        } else if (errno != ENOENT)
            return fail(error, "Examinar destino", errno);
        bool allowed = step("rollback-publicar", error);
        if (!allowed || rename(backup.c_str(), target.c_str()) != 0) {
            std::string cause = allowed ? "Restaurar backup errno=" + std::to_string(errno) : error;
            if (matches(reverted, t.manifest.app.sha256, t.manifest.app.size) &&
                lstat(target.c_str(), &st) != 0 && errno == ENOENT) {
                if (rename(reverted.c_str(), target.c_str()) == 0)
                    cause += "; nova versao restaurada";
                else
                    cause += "; recuperacao errno=" + std::to_string(errno);
            }
            return fail(error, cause);
        }
        return matches(target, t.oldHash, t.oldSize) ? true : fail(error, "Conferir rollback");
    }
    if (currentNew)
        return true;
    if (!validateNro(next, t.manifest, false, error)) {
        if (backupOld && lstat(target.c_str(), &st) != 0 && errno == ENOENT) {
            if (rename(backup.c_str(), target.c_str()) != 0)
                error += "; recuperar backup errno=" + std::to_string(errno);
            else
                error += "; anterior restaurado";
        }
        return false;
    }
    if (!space(t.directory, 0, error)) {
        if (backupOld && lstat(target.c_str(), &st) != 0 && errno == ENOENT) {
            if (rename(backup.c_str(), target.c_str()) == 0)
                error += "; anterior restaurado";
            else
                error += "; recuperar backup errno=" + std::to_string(errno);
        }
        return false;
    }
    if (currentOld) {
        if (lstat(backup.c_str(), &st) == 0 || errno != ENOENT)
            return fail(error, "Backup preexistente preservado");
        if (!step("antes-backup", error) || rename(target.c_str(), backup.c_str()) != 0)
            return fail(error, "Reservar backup", errno);
        if (!step("apos-backup", error)) {
            if (rename(backup.c_str(), target.c_str()) != 0)
                error += "; recuperar backup errno=" + std::to_string(errno);
            else
                error += "; anterior restaurado";
            return false;
        }
        backupOld = matches(backup, t.oldHash, t.oldSize);
    } else if (lstat(target.c_str(), &st) == 0 || errno != ENOENT)
        return fail(error, "NRO alvo mudou; instalacao bloqueada");
    if (!backupOld)
        return fail(error, "Backup/espaco nao verificavel");
    bool allowed = step("publicar-nro", error);
    if (!allowed || rename(next.c_str(), target.c_str()) != 0) {
        std::string cause = allowed ? "Publicar NRO errno=" + std::to_string(errno) : error;
        if (lstat(target.c_str(), &st) != 0 && errno == ENOENT) {
            if (rename(backup.c_str(), target.c_str()) == 0)
                cause += "; anterior restaurado";
            else
                cause += "; recuperar backup errno=" + std::to_string(errno);
        }
        return fail(error, cause);
    }
    if (!step("apos-publicar", error))
        return false;
    return matches(target, t.manifest.app.sha256, t.manifest.app.size)
               ? true
               : fail(error, "Conferir NRO instalado");
}
#ifdef HOST_TEST
void testFault(const std::string &stage, int code, bool crash) {
    fault = stage;
    faultCode = code;
    crashFault = crash;
}
#endif
} // namespace sfm::updates
