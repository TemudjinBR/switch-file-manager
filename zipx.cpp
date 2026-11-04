#include "zipx.h"
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "miniz.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace zx {

static std::string g_err;
std::string lastError() { return g_err; }

static bool isDirPath(const std::string& p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

static std::string joinPath(const std::string& a, const std::string& b) {
    if (!a.empty() && a.back() == '/') return a + b;
    return a + "/" + b;
}

static std::string baseOf(std::string p) {
    while (p.size() > 1 && p.back() == '/') p.pop_back();
    size_t i = p.find_last_of('/');
    return i == std::string::npos ? p : p.substr(i + 1);
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
static bool safeComponents(const std::string& raw, std::vector<std::string>& out) {
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

static void mkdirs(const std::string& p) {
    // cria cada nível, ignorando "já existe"
    size_t start = p.find(":/");
    start = start == std::string::npos ? 1 : start + 2;
    for (size_t i = start; i <= p.size(); i++) {
        if (i == p.size() || p[i] == '/') {
            std::string sub = p.substr(0, i);
            if (!sub.empty()) mkdir(sub.c_str(), 0777);
        }
    }
}

bool inspect(const std::string& zipPath, Info& out) {
    mz_zip_archive z;
    memset(&z, 0, sizeof z);
    if (!mz_zip_reader_init_file(&z, zipPath.c_str(), 0)) {
        g_err = "Não foi possível abrir o ZIP (arquivo inválido ou corrompido).";
        return false;
    }
    out = Info();
    mz_uint n = mz_zip_reader_get_num_files(&z);
    for (mz_uint i = 0; i < n; i++) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&z, i, &st)) continue;
        if (st.m_is_directory) { out.dirs++; continue; }
        out.files++;
        out.totalBytes += st.m_uncomp_size;
        if (st.m_is_encrypted) out.encrypted = true;
    }
    mz_zip_reader_end(&z);
    return true;
}

bool extract(const std::string& zipPath, const std::string& destDir, Progress* pr, int& skipped) {
    skipped = 0;
    mz_zip_archive z;
    memset(&z, 0, sizeof z);
    if (!mz_zip_reader_init_file(&z, zipPath.c_str(), 0)) {
        g_err = "Não foi possível abrir o ZIP (arquivo inválido ou corrompido).";
        return false;
    }
    mkdirs(destDir);
    const size_t BS = 256 * 1024;
    char* buf = (char*)malloc(BS);
    if (!buf) { mz_zip_reader_end(&z); g_err = "Memória insuficiente."; return false; }

    bool ok = true;
    mz_uint n = mz_zip_reader_get_num_files(&z);
    for (mz_uint i = 0; i < n && ok; i++) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&z, i, &st)) { skipped++; continue; }
        std::vector<std::string> comps;
        if (!safeComponents(st.m_filename, comps)) { skipped++; continue; }
        std::string path = destDir;
        for (auto& c : comps) path = joinPath(path, c);

        if (st.m_is_directory) { mkdirs(path); continue; }
        if (st.m_is_encrypted || !st.m_is_supported || (st.m_method != 0 && st.m_method != MZ_DEFLATED)) { skipped++; continue; }

        std::string parent = path.substr(0, path.find_last_of('/'));
        mkdirs(parent);

        FILE* f = fopen(path.c_str(), "wb");
        if (!f) { g_err = "Não foi possível criar: " + comps.back(); ok = false; break; }
        mz_zip_reader_extract_iter_state* it = mz_zip_reader_extract_iter_new(&z, i, 0);
        if (!it) { fclose(f); remove(path.c_str()); skipped++; continue; }
        bool fileOk = true;
        for (;;) {
            size_t got = mz_zip_reader_extract_iter_read(it, buf, BS);
            if (got == 0) break;
            if (fwrite(buf, 1, got, f) != got) {
                g_err = "Falha ao gravar (cartão cheio, ou limite de 4 GB por arquivo no formato FAT32).";
                fileOk = false;
                break;
            }
            if (pr && !pr->tick(got, comps.back().c_str())) { g_err = "Cancelado."; fileOk = false; ok = false; break; }
        }
        bool crcOk = mz_zip_reader_extract_iter_free(it);   // confere o CRC ao terminar
        fclose(f);
        if (fileOk && !crcOk) { g_err = "O arquivo \"" + comps.back() + "\" está corrompido dentro do ZIP."; fileOk = false; }
        if (!fileOk) { remove(path.c_str()); ok = false; }
    }
    free(buf);
    mz_zip_reader_end(&z);
    return ok;
}

struct ReadCtx {
    FILE* f;
    Progress* pr;
    const char* name;
    bool cancelled;
    uint64_t reached;  // maior posição do arquivo que foi lida
};

static size_t readCb(void* opaque, mz_uint64 ofs, void* b, size_t n) {
    ReadCtx* c = (ReadCtx*)opaque;
    if (fseeko(c->f, (off_t)ofs, SEEK_SET) != 0) return 0;
    size_t got = fread(b, 1, n, c->f);
    if (ofs + got > c->reached) c->reached = ofs + got;
    if (got > 0 && c->pr && !c->pr->tick(got, c->name)) { c->cancelled = true; return 0; }
    return got;
}

static std::vector<std::string> listDir(const std::string& d) {
    std::vector<std::string> v;
    DIR* dir = opendir(d.c_str());
    if (!dir) return v;
    while (dirent* e = readdir(dir)) {
        std::string n = e->d_name;
        if (n != "." && n != "..") v.push_back(n);
    }
    closedir(dir);
    std::sort(v.begin(), v.end());
    return v;
}

static uint64_t treeBytes(const std::string& p) {
    struct stat st;
    if (stat(p.c_str(), &st) != 0) return 0;
    if (!S_ISDIR(st.st_mode)) return (uint64_t)st.st_size;
    uint64_t t = 0;
    for (auto& n : listDir(p)) t += treeBytes(joinPath(p, n));
    return t;
}

static bool addTree(mz_zip_archive* z, const std::string& fsPath, const std::string& arcPath, int level, Progress* pr, const std::string& zipPath) {
    if (fsPath == zipPath) return true;  // não coloca o próprio zip dentro dele
    if (isDirPath(fsPath)) {
        std::string dirName = arcPath + "/";
        if (!mz_zip_writer_add_mem(z, dirName.c_str(), nullptr, 0, 0)) { g_err = "Falha ao registrar a pasta no ZIP."; return false; }
        for (auto& n : listDir(fsPath))
            if (!addTree(z, joinPath(fsPath, n), arcPath + "/" + n, level, pr, zipPath)) return false;
        return true;
    }
    FILE* f = fopen(fsPath.c_str(), "rb");
    if (!f) { g_err = "Não foi possível abrir: " + baseOf(fsPath); return false; }
    fseeko(f, 0, SEEK_END);
    uint64_t size = (uint64_t)ftello(f);
    fseeko(f, 0, SEEK_SET);
    ReadCtx ctx;
    ctx.f = f;
    ctx.pr = pr;
    std::string nm = baseOf(fsPath);
    ctx.name = nm.c_str();
    ctx.cancelled = false;
    ctx.reached = 0;
    mz_bool r = mz_zip_writer_add_read_buf_callback(z, arcPath.c_str(), readCb, &ctx, size, nullptr, nullptr, 0, (mz_uint)level,
                                                    nullptr, 0, nullptr, 0);
    fclose(f);
    if (ctx.cancelled) { g_err = "Cancelado."; return false; }
    if (!r || ctx.reached != size) {
        g_err = "Falha ao compactar: " + nm;
        return false;
    }
    return true;
}

bool compress(const std::vector<std::string>& paths, const std::string& zipPath, int level, Progress* pr) {
    uint64_t total = 0;
    for (auto& p : paths) total += treeBytes(p);
    mz_uint flags = total > 0xF0000000ull ? MZ_ZIP_FLAG_WRITE_ZIP64 : 0;

    mz_zip_archive z;
    memset(&z, 0, sizeof z);
    if (!mz_zip_writer_init_file_v2(&z, zipPath.c_str(), 0, flags)) {
        g_err = "Não foi possível criar o arquivo ZIP aqui.";
        return false;
    }
    bool ok = true;
    for (auto& p : paths) {
        if (!addTree(&z, p, baseOf(p), level, pr, zipPath)) { ok = false; break; }
    }
    if (ok && !mz_zip_writer_finalize_archive(&z)) { g_err = "Falha ao finalizar o ZIP."; ok = false; }
    mz_zip_writer_end(&z);
    if (!ok) remove(zipPath.c_str());
    return ok;
}

}  // namespace zx
