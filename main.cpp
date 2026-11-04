// Switch File Manager v0.3.0 - interface Commander, dois paineis e operacoes seguras
// C++ / libnx (renderização por software, sem bibliotecas externas) + Google Drive

#include <switch.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <errno.h>
#include <unistd.h>
#include <cmath>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <algorithm>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"
#include "gdrive.h"
#include "zipx.h"

struct Col { uint8_t r, g, b, a; };
static const int W = 1280, H = 720;
static const int ROWS = 10, ROW_H = 46, LIST_TOP = 116, PANE_Y = 72, PANE_H = 560, PANE_W = 610;

static const Col C_BG    = {11, 15, 24, 255};
static const Col C_PANEL = {22, 29, 43, 255};
static const Col C_PAN2  = {35, 47, 66, 255};
static const Col C_SEL   = {43, 57, 77, 255};
static const Col C_ACC   = {62, 174, 241, 255};
static const Col C_ACCD  = {35, 105, 156, 255};
static const Col C_TXT   = {232, 237, 247, 255};
static const Col C_DIM   = {125, 137, 163, 255};
static const Col C_RED   = {244, 106, 106, 255};
static const Col C_GRN   = {84, 214, 140, 255};
static const Col C_YEL   = {250, 204, 21, 255};

static Framebuffer g_fb;
static uint32_t* canvas = nullptr;
struct Fnt { int px; float scale; int asc; };
static Fnt fS, fM, fL;
static stbtt_fontinfo g_info;
static bool g_fontOk = false;
static PadState pad;
static bool g_quit = false, g_cancel = false;
static u64 g_free = 0, g_total = 0;

// ---------------------------------------------------------------- utilidades

static std::string join(const std::string& a, const std::string& b) {
    if (!a.empty() && a.back() == '/') return a + b;
    return a + "/" + b;
}

static bool pathExists(const std::string& p) { struct stat st; return stat(p.c_str(), &st) == 0; }
static bool isDir(const std::string& p) { struct stat st; return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode); }

static std::string baseName(std::string p) {
    while (p.size() > 1 && p.back() == '/') p.pop_back();
    size_t i = p.find_last_of('/');
    return i == std::string::npos ? p : p.substr(i + 1);
}

static std::string parentOf(std::string s) {
    while (s.size() > 1 && s.back() == '/') s.pop_back();
    size_t i = s.find_last_of('/');
    if (i == std::string::npos) return "sdmc:/";
    std::string r = s.substr(0, i + 1);
    if (r.size() > 6) r.pop_back();
    if (r.size() < 6) return "sdmc:/";
    return r;
}

static std::string uniqueName(const std::string& p) {
    if (!pathExists(p)) return p;
    size_t dot = p.find_last_of('.'), sl = p.find_last_of('/');
    std::string base = p, ext;
    if (dot != std::string::npos && (sl == std::string::npos || dot > sl + 1)) {
        base = p.substr(0, dot);
        ext = p.substr(dot);
    }
    for (int i = 1; i < 1000; i++) {
        std::string c = base + " (" + std::to_string(i) + ")" + ext;
        if (!pathExists(c)) return c;
    }
    return p;
}

static std::string fmtSize(u64 b) {
    char buf[32];
    if (b < 1024ULL) snprintf(buf, sizeof buf, "%llu B", (unsigned long long)b);
    else if (b < 1024ULL * 1024) snprintf(buf, sizeof buf, "%.1f KB", b / 1024.0);
    else if (b < 1024ULL * 1024 * 1024) snprintf(buf, sizeof buf, "%.1f MB", b / 1048576.0);
    else snprintf(buf, sizeof buf, "%.2f GB", b / 1073741824.0);
    return buf;
}

static std::string lower(std::string s) {
    for (auto& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

static std::vector<std::string> splitLines(const std::string& s) {
    std::vector<std::string> r;
    std::string cur;
    for (char c : s) {
        if (c == '\n') { r.push_back(cur); cur.clear(); }
        else cur += c;
    }
    if (!cur.empty()) r.push_back(cur);
    return r;
}

static std::vector<std::string> listNames(const std::string& dirPath) {
    std::vector<std::string> names;
    DIR* d = opendir(dirPath.c_str());
    if (!d) return names;
    while (dirent* e = readdir(d)) {
        std::string n = e->d_name;
        if (n == "." || n == "..") continue;
        names.push_back(n);
    }
    closedir(d);
    return names;
}

static u64 treeSize(const std::string& p) {
    struct stat st;
    if (stat(p.c_str(), &st) != 0) return 0;
    if (!S_ISDIR(st.st_mode)) return (u64)st.st_size;
    u64 t = 0;
    for (auto& n : listNames(p)) t += treeSize(join(p, n));
    return t;
}

static bool isCritical(std::string p) {
    while (p.size() > 1 && p.back() == '/') p.pop_back();
    static const char* crit[] = {"sdmc:/atmosphere", "sdmc:/bootloader", "sdmc:/emuMMC", "sdmc:/Nintendo", "sdmc:/switch"};
    for (auto c : crit) if (p == c) return true;
    return false;
}

static void refreshSpace() {
    struct statvfs sv;
    if (statvfs("sdmc:/", &sv) == 0) {
        g_free = (u64)sv.f_bavail * sv.f_frsize;
        g_total = (u64)sv.f_blocks * sv.f_frsize;
    }
}

// ---------------------------------------------------------------- painéis

// ---------------------------------------------------------------- favoritos e histórico

static std::vector<std::string> g_favs, g_hist;
static const char* CFG_DIR = "sdmc:/switch/switch-manager";
static const int HIST_MAX = 15;

static std::string cfgFile(const char* name) { return std::string(CFG_DIR) + "/" + name; }

static std::vector<std::string> loadLines(const char* name) {
    std::vector<std::string> v;
    FILE* f = fopen(cfgFile(name).c_str(), "rb");
    if (!f) return v;
    std::string cur;
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c == '\n') { if (!cur.empty()) v.push_back(cur); cur.clear(); }
        else if (c != '\r') cur += (char)c;
    }
    if (!cur.empty()) v.push_back(cur);
    fclose(f);
    return v;
}

static void saveLines(const char* name, const std::vector<std::string>& v) {
    mkdir("sdmc:/switch", 0777);
    mkdir(CFG_DIR, 0777);
    FILE* f = fopen(cfgFile(name).c_str(), "wb");
    if (!f) return;
    for (auto& l : v) { fputs(l.c_str(), f); fputc('\n', f); }
    fclose(f);
}

static void historyAdd(const std::string& path) {
    if (path == "sdmc:/" || path.compare(0, 5, "sdmc:") != 0) return;
    if (!g_hist.empty() && g_hist.front() == path) return;
    g_hist.erase(std::remove(g_hist.begin(), g_hist.end(), path), g_hist.end());
    g_hist.insert(g_hist.begin(), path);
    if ((int)g_hist.size() > HIST_MAX) g_hist.resize(HIST_MAX);
    saveLines("historico.txt", g_hist);
}

struct Entry {
    std::string name;
    std::string key;    // chave única (local: o nome; Drive: o id)
    std::string id;     // somente no Drive
    bool dir = false;
    bool gdoc = false;  // arquivo nativo do Google (Docs, Planilhas...): não baixável
    u64 size = 0;
    time_t mtime = 0;
};

struct Pane;
static bool driveLoad(Pane& p);

struct Pane {
    std::string path = "sdmc:/";
    bool drive = false;
    std::vector<std::pair<std::string, std::string>> dstack;  // (id, nome) das pastas abertas no Drive
    std::vector<Entry> items;
    int cur = 0, scroll = 0;
    std::set<std::string> marks;
    int sortMode = 0;        // 0 nome, 1 tamanho, 2 data, 3 tipo
    bool sortDesc = false;
    std::string filter;      // busca por nome (vazio = sem filtro)

    static std::string extOf(const std::string& n) {
        size_t d = n.find_last_of('.');
        return (d == std::string::npos || d == 0) ? std::string() : lower(n.substr(d + 1));
    }
    void sortItems() {
        auto nameCmp = [](const Entry& a, const Entry& b) {
            std::string la = lower(a.name), lb = lower(b.name);
            return la < lb ? -1 : (la > lb ? 1 : 0);
        };
        std::sort(items.begin(), items.end(), [&](const Entry& a, const Entry& b) {
            if (a.dir != b.dir) return a.dir;
            int r = 0;
            switch (sortMode) {
                case 1: if (!a.dir && a.size != b.size) r = a.size < b.size ? -1 : 1; break;
                case 2: if (a.mtime != b.mtime) r = a.mtime < b.mtime ? -1 : 1; break;
                case 3: { std::string ea = extOf(a.name), eb = extOf(b.name); if (ea != eb) r = ea < eb ? -1 : 1; break; }
                default: r = nameCmp(a, b); break;
            }
            if (sortDesc) r = -r;
            if (r == 0) r = nameCmp(a, b);
            return r < 0;
        });
    }
    void applyFilter() {
        if (filter.empty()) return;
        std::string f = lower(filter);
        std::vector<Entry> keep;
        for (auto& e : items)
            if (lower(e.name).find(f) != std::string::npos) keep.push_back(e);
        items.swap(keep);
    }
    std::string sortLabel() const {
        static const char* nm[] = {"Nome", "Tamanho", "Data", "Tipo"};
        static const char* asc[] = {"A-Z", "menor", "antigo", "A-Z"};
        static const char* dsc[] = {"Z-A", "maior", "novo", "Z-A"};
        return std::string(nm[sortMode]) + " (" + (sortDesc ? dsc[sortMode] : asc[sortMode]) + ")";
    }

    std::string driveId() const { return dstack.empty() ? "root" : dstack.back().first; }
    void setDrivePath() {
        path = "Google Drive:/";
        for (size_t i = 0; i < dstack.size(); i++) {
            path += dstack[i].second;
            if (i + 1 < dstack.size()) path += "/";
        }
    }

    void reload(const std::string& focus = "") {
        items.clear();
        if (drive) {
            driveLoad(*this);
        } else {
            historyAdd(path);
            for (auto& n : listNames(path)) {
                struct stat st;
                if (stat(join(path, n).c_str(), &st) != 0) continue;
                Entry e;
                e.name = n;
                e.key = n;
                e.dir = S_ISDIR(st.st_mode);
                e.size = (u64)st.st_size;
                e.mtime = st.st_mtime;
                items.push_back(e);
            }
        }
        sortItems();
        applyFilter();
        marks.clear();
        int old = cur;
        cur = std::min(old, (int)items.size() - 1);
        if (cur < 0) cur = 0;
        if (!focus.empty()) {
            for (int i = 0; i < (int)items.size(); i++)
                if (items[i].name == focus) { cur = i; scroll = std::max(0, i - 3); break; }
        }
        clamp();
    }
    int maxScroll() const { return std::max(0, (int)items.size() - ROWS); }
    void clamp() {
        if (scroll > maxScroll()) scroll = maxScroll();
        if (scroll < 0) scroll = 0;
    }
    void ensureVisible() {
        if (cur < scroll) scroll = cur;
        if (cur >= scroll + ROWS) scroll = cur - ROWS + 1;
        clamp();
    }
};

static Pane panes[2];
static int active = 0;

static std::vector<Entry> selection(Pane& p) {
    std::vector<Entry> r;
    if (!p.marks.empty()) {
        for (auto& e : p.items) if (p.marks.count(e.key)) r.push_back(e);
    } else if (!p.items.empty()) {
        r.push_back(p.items[p.cur]);
    }
    return r;
}

// ---------------------------------------------------------------- desenho

static int clipX0 = 0, clipY0 = 0, clipX1 = W, clipY1 = H;
static void setClip(int x, int y, int w, int h) {
    clipX0 = std::max(0, x); clipY0 = std::max(0, y);
    clipX1 = std::min(W, x + w); clipY1 = std::min(H, y + h);
}
static void resetClip() { clipX0 = 0; clipY0 = 0; clipX1 = W; clipY1 = H; }

static inline void blendPx(uint32_t* p, Col c, int a) {
    uint32_t d = *p;
    int dr = d & 255, dg = (d >> 8) & 255, db = (d >> 16) & 255;
    int r = (c.r * a + dr * (255 - a)) / 255;
    int g = (c.g * a + dg * (255 - a)) / 255;
    int b = (c.b * a + db * (255 - a)) / 255;
    *p = 0xFF000000u | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
}

static void putPx(int x, int y, Col c) {
    if (x < clipX0 || x >= clipX1 || y < clipY0 || y >= clipY1 || c.a == 0) return;
    blendPx(&canvas[y * W + x], c, c.a);
}

static void hspan(int y, int xa, int xb, Col c) {
    if (y < clipY0 || y >= clipY1) return;
    xa = std::max(xa, clipX0);
    xb = std::min(xb, clipX1);
    if (xa >= xb) return;
    uint32_t* row = canvas + y * W;
    if (c.a == 255) {
        uint32_t v = 0xFF000000u | ((uint32_t)c.b << 16) | ((uint32_t)c.g << 8) | (uint32_t)c.r;
        for (int x = xa; x < xb; x++) row[x] = v;
    } else {
        for (int x = xa; x < xb; x++) blendPx(&row[x], c, c.a);
    }
}

static void fillRect(int x, int y, int w, int h, Col c) {
    for (int j = y; j < y + h; j++) hspan(j, x, x + w, c);
}

static void fillRound(int x, int y, int w, int h, int r, Col c) {
    if (r * 2 > h) r = h / 2;
    if (r * 2 > w) r = w / 2;
    fillRect(x + r, y, w - 2 * r, h, c);
    fillRect(x, y + r, r, h - 2 * r, c);
    fillRect(x + w - r, y + r, r, h - 2 * r, c);
    for (int j = 0; j < r; j++) {
        for (int i = 0; i < r; i++) {
            float dx = (float)r - i - 0.5f, dy = (float)r - j - 0.5f;
            float cov = (float)r - sqrtf(dx * dx + dy * dy) + 0.5f;
            if (cov <= 0) continue;
            if (cov > 1) cov = 1;
            Col cc = c;
            cc.a = (uint8_t)(c.a * cov);
            putPx(x + i, y + j, cc);
            putPx(x + w - 1 - i, y + j, cc);
            putPx(x + i, y + h - 1 - j, cc);
            putPx(x + w - 1 - i, y + h - 1 - j, cc);
        }
    }
}

// ---- texto (stb_truetype)
static Fnt makeFnt(int px) {
    Fnt f;
    f.px = px;
    f.scale = stbtt_ScaleForMappingEmToPixels(&g_info, (float)px);
    int a, d, l;
    stbtt_GetFontVMetrics(&g_info, &a, &d, &l);
    f.asc = (int)lroundf(a * f.scale);
    return f;
}

struct Glyph { unsigned char* bmp; int w, h, xo, yo; float adv; };
static std::map<uint64_t, Glyph> g_glyphs;

static Glyph& getGlyph(const Fnt& f, uint32_t cp) {
    uint64_t key = ((uint64_t)f.px << 32) | cp;
    auto it = g_glyphs.find(key);
    if (it != g_glyphs.end()) return it->second;
    Glyph g = {nullptr, 0, 0, 0, 0, 0.f};
    int gi = stbtt_FindGlyphIndex(&g_info, (int)cp);
    int adv = 0, lsb = 0;
    stbtt_GetGlyphHMetrics(&g_info, gi, &adv, &lsb);
    g.adv = adv * f.scale;
    g.bmp = stbtt_GetGlyphBitmap(&g_info, f.scale, f.scale, gi, &g.w, &g.h, &g.xo, &g.yo);
    g_glyphs[key] = g;
    return g_glyphs[key];
}

static uint32_t nextCp(const char*& s) {
    unsigned char c = (unsigned char)*s++;
    if (c < 0x80) return c;
    int n = (c >> 5) == 6 ? 1 : (c >> 4) == 14 ? 2 : (c >> 3) == 30 ? 3 : 0;
    if (n == 0) return '?';
    uint32_t cp = n == 1 ? (c & 0x1F) : n == 2 ? (c & 0x0F) : (c & 0x07);
    for (int i = 0; i < n; i++) {
        if (((unsigned char)*s & 0xC0) != 0x80) return '?';
        cp = (cp << 6) | ((unsigned char)*s++ & 0x3F);
    }
    return cp;
}

static int textWidth(const Fnt& f, const std::string& str) {
    const char* s = str.c_str();
    float w = 0;
    while (*s) w += getGlyph(f, nextCp(s)).adv;
    return (int)(w + 0.5f);
}

static std::string fitText(const Fnt& f, std::string s, int maxW) {
    if (textWidth(f, s) <= maxW) return s;
    while (!s.empty()) {
        size_t i = s.size() - 1;
        while (i > 0 && (((unsigned char)s[i]) & 0xC0) == 0x80) i--;
        s.erase(i);
        std::string t = s + "...";
        if (textWidth(f, t) <= maxW) return t;
    }
    return "";
}

// align: 0 esquerda, 1 direita, 2 centro
static void drawText(const Fnt& f, std::string s, int x, int y, Col c, int maxW = 0, int align = 0) {
    if (maxW > 0) s = fitText(f, s, maxW);
    int tw = textWidth(f, s);
    if (align == 1) x -= tw;
    else if (align == 2) x -= tw / 2;
    int base = y + f.asc;
    float pen = (float)x;
    const char* p = s.c_str();
    while (*p) {
        Glyph& g = getGlyph(f, nextCp(p));
        if (g.bmp) {
            int px = (int)(pen + 0.5f) + g.xo, py = base + g.yo;
            for (int j = 0; j < g.h; j++) {
                int yy = py + j;
                if (yy < clipY0 || yy >= clipY1) continue;
                for (int i = 0; i < g.w; i++) {
                    int xx = px + i;
                    if (xx < clipX0 || xx >= clipX1) continue;
                    int a = g.bmp[j * g.w + i] * c.a / 255;
                    if (a) blendPx(&canvas[yy * W + xx], c, a);
                }
            }
        }
        pen += g.adv;
    }
}

static void present() {
    u32 stride;
    uint32_t* fb = (uint32_t*)framebufferBegin(&g_fb, &stride);
    for (int y = 0; y < H; y++) memcpy((uint8_t*)fb + (size_t)y * stride, canvas + (size_t)y * W, W * 4);
    framebufferEnd(&g_fb);
}

static void drawPane(Pane& p, int x, bool act) {
    int y = PANE_Y, w = PANE_W, h = PANE_H;
    if (act) fillRound(x - 3, y - 3, w + 6, h + 6, 17, C_ACC);
    fillRound(x, y, w, h, 14, C_PANEL);
    fillRound(x + 12, y + 5, 5, 29, 2, act ? C_GRN : C_DIM);

    {
        std::string sl = p.sortLabel();
        int sw = textWidth(fS, sl);
        drawText(fS, sl, x + w - 18, y + 9, C_DIM, 0, 1);
        drawText(fS, p.path, x + 27, y + 9, p.drive ? (act ? C_GRN : C_DIM) : (act ? C_TXT : C_DIM), w - 45 - sw - 16);
    }
    fillRect(x + 12, y + 42, w - 24, 2, C_PAN2);

    setClip(x, LIST_TOP, w, ROWS * ROW_H + 6);

    if (p.items.empty()) {
        drawText(fM, "Pasta vazia", x + w / 2, LIST_TOP + 150, C_DIM, 0, 2);
    }
    for (int i = p.scroll; i < (int)p.items.size() && i < p.scroll + ROWS + 1; i++) {
        Entry& e = p.items[i];
        int ry = LIST_TOP + (i - p.scroll) * ROW_H;
        if (i == p.cur) fillRound(x + 8, ry + 2, w - 16, ROW_H - 4, 10, act ? C_ACCD : C_SEL);
        if (p.marks.count(e.key)) {
            fillRound(x + 10, ry + 8, 7, ROW_H - 16, 3, C_YEL);
            fillRound(x + 19, ry + 15, 4, ROW_H - 30, 2, C_YEL);
        }
        if (e.dir) {
            fillRound(x + 26, ry + 13, 26, 20, 5, C_ACC);
            fillRound(x + 26, ry + 10, 12, 8, 3, C_ACC);
        } else {
            fillRound(x + 28, ry + 10, 21, 26, 4, C_DIM);
        }
        bool showDate = p.sortMode == 2 && e.mtime > 0;
        drawText(fM, e.name, x + 64, ry + 8, e.gdoc ? C_DIM : C_TXT, w - 64 - (showDate ? 250 : 130));
        if (showDate) {
            char db[24];
            time_t tt = e.mtime;
            struct tm* lt = localtime(&tt);
            if (lt) {
                strftime(db, sizeof db, "%d/%m/%Y", lt);
                drawText(fS, db, x + w - (e.dir ? 20 : 130), ry + 11, C_DIM, 0, 1);
            }
        }
        if (e.gdoc) drawText(fS, "Google", x + w - 20, ry + 11, C_DIM, 0, 1);
        else if (!e.dir) drawText(fS, fmtSize(e.size), x + w - 20, ry + 11, C_DIM, 0, 1);
    }
    resetClip();

    if ((int)p.items.size() > ROWS) {
        int trackH = ROWS * ROW_H - 8;
        int thumbH = std::max(30, trackH * ROWS / (int)p.items.size());
        int thumbY = LIST_TOP + 4 + (trackH - thumbH) * p.scroll / std::max(1, p.maxScroll());
        fillRound(x + w - 8, thumbY, 4, thumbH, 2, C_DIM);
    }
}

struct Btn { const char* key; const char* label; int id; };
enum { B_OPEN, B_UP, B_MARK, B_ACT, B_PANE, B_CLOUD, B_EXIT };
static const int NBTN = 7;
static const Btn BTNS[NBTN] = {
    {"A", "Abrir", B_OPEN}, {"B", "Voltar", B_UP}, {"Y", "Marcar", B_MARK}, {"X", "Ações", B_ACT},
    {"L/R", "Painel", B_PANE}, {"-", "Nuvem", B_CLOUD}, {"+", "Sair", B_EXIT}};

static void drawMain() {
    fillRect(0, 0, W, H, C_BG);

    fillRound(20, 15, 6, 35, 3, C_RED);
    fillRound(29, 15, 6, 35, 3, C_ACC);
    drawText(fL, "Switch File Manager", 47, 14, C_TXT);
    {
        int n = (int)selection(panes[active]).size();
        if (!panes[active].marks.empty())
            drawText(fS, std::to_string(n) + " marcado(s)", 370, 24, C_YEL);
    }
    if (!panes[active].filter.empty())
        drawText(fS, "Busca: " + panes[active].filter, 600, 24, C_YEL, 260);
    if (g_total > 0) {
        std::string t = "Livre " + fmtSize(g_free) + " de " + fmtSize(g_total);
        drawText(fS, t, W - 24, 14, C_DIM, 0, 1);
        fillRound(W - 224, 46, 200, 8, 4, C_PAN2);
        int used = (int)(200.0 * (double)(g_total - g_free) / (double)g_total);
        if (used > 0) fillRound(W - 224, 46, std::max(8, used), 8, 4, C_ACC);
    }

    drawPane(panes[0], 20, active == 0);
    drawPane(panes[1], 650, active == 1);

    for (int i = 0; i < NBTN; i++) {
        int bx = 22 + i * 178;
        fillRound(bx, 646, 168, 58, 12, C_PANEL);
        fillRound(bx + 8, 655, 44, 39, 9, C_PAN2);
        int kw = textWidth(fM, BTNS[i].key);
        int lx = bx + 58;
        drawText(fM, BTNS[i].key, bx + 30, 660, C_ACC, 40, 2);
        drawText(fM, BTNS[i].label, lx, 660, C_TXT, bx + 160 - lx);
    }
}

// ---------------------------------------------------------------- entrada

struct Touch { bool down = false, pressed = false, released = false, moved = false; int x = 0, y = 0, sx = 0, sy = 0, ly = 0; };
static Touch tp;

static void pump() {}

static void updateTouch() {
    HidTouchScreenState st = {0};
    hidGetTouchScreenStates(&st, 1);
    bool now = st.count > 0;
    tp.pressed = now && !tp.down;
    tp.released = !now && tp.down;
    if (now) {
        tp.x = (int)st.touches[0].x;
        tp.y = (int)st.touches[0].y;
        if (tp.pressed) { tp.sx = tp.x; tp.sy = tp.y; tp.ly = tp.y; tp.moved = false; }
        else if (abs(tp.x - tp.sx) > 20 || abs(tp.y - tp.sy) > 20) tp.moved = true;
    }
    tp.down = now;
}

static bool inRect(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

// ---------------------------------------------------------------- janelas

static int modal(const std::string& title, const std::string& text, const std::vector<std::string>& opts, int sel = 0) {
    std::vector<std::string> lines = splitLines(text);
    const int bw = 640, lineH = 30;
    int n = (int)opts.size();
    if (n == 0) return -1;
    int rowH = n > 8 ? 46 : 54;
    int textH = lines.empty() ? 0 : (int)lines.size() * lineH + 14;
    int maxVis = std::max(3, (H - 40 - 84 - textH - 22) / rowH);
    int vis = std::min(n, maxVis);
    int bh = 84 + textH + vis * rowH + 22;
    int bx = (W - bw) / 2, by = (H - bh) / 2;
    int oy0 = by + 84 + textH;
    if (sel < 0 || sel >= n) sel = 0;
    int top = 0;
    if (sel >= vis) top = sel - vis + 1;

    for (;;) {
        if (!appletMainLoop()) { g_quit = true; return -1; }
        padUpdate(&pad);
        updateTouch();
        pump();
        u64 d = padGetButtonsDown(&pad);
        if (d & HidNpadButton_B) return -1;
        if (d & (HidNpadButton_Up | HidNpadButton_StickLUp)) sel = (sel + n - 1) % n;
        if (d & (HidNpadButton_Down | HidNpadButton_StickLDown)) sel = (sel + 1) % n;
        if (d & HidNpadButton_ZL) sel = std::max(0, sel - vis);
        if (d & HidNpadButton_ZR) sel = std::min(n - 1, sel + vis);
        if (sel < top) top = sel;
        if (sel >= top + vis) top = sel - vis + 1;
        if (d & HidNpadButton_A) return sel;
        // arrastar a lista com o dedo
        if (n > vis && tp.down && tp.moved && inRect(tp.sx, tp.sy, bx, oy0, bw, vis * rowH)) {
            static int accum = 0;
            accum += tp.ly - tp.y;
            tp.ly = tp.y;
            while (accum >= rowH && top < n - vis) { top++; accum -= rowH; }
            while (accum <= -rowH && top > 0) { top--; accum += rowH; }
        }
        if (tp.released && !tp.moved) {
            for (int i = 0; i < vis; i++)
                if (inRect(tp.x, tp.y, bx + 20, oy0 + i * rowH, bw - 40, rowH - 6)) return top + i;
            if (!inRect(tp.x, tp.y, bx, by, bw, bh)) return -1;
        }

        drawMain();
        fillRect(0, 0, W, H, Col{0, 0, 0, 175});
        fillRound(bx, by, bw, bh, 18, C_PANEL);
        drawText(fL, title, bx + 28, by + 22, C_TXT, bw - 56);
        for (int i = 0; i < (int)lines.size(); i++)
            drawText(fM, lines[i], bx + 28, by + 80 + i * lineH, C_DIM, bw - 56);
        for (int k = 0; k < vis; k++) {
            int i = top + k;
            int oy = oy0 + k * rowH;
            bool danger = opts[i].rfind("Apagar", 0) == 0;
            fillRound(bx + 20, oy, bw - 40, rowH - 6, 12, i == sel ? C_ACCD : C_PAN2);
            drawText(fM, opts[i], bx + bw / 2, oy + (rowH - 54) / 2 + 9, danger ? C_RED : C_TXT, bw - 90, 2);
        }
        if (n > vis) {
            int trackH = vis * rowH - 6;
            int thumbH = std::max(24, trackH * vis / n);
            int thumbY = oy0 + (trackH - thumbH) * top / std::max(1, n - vis);
            fillRound(bx + bw - 14, thumbY, 4, thumbH, 2, C_DIM);
        }
        present();
    }
}

static void message(const std::string& title, const std::string& text) {
    modal(title, text, {"OK"});
}

// progresso
static std::string g_pTitle, g_pName, g_err;
static u64 g_pDone = 0, g_pTotal = 0, g_lastUI = 0;

static void drawProgressOverlay() {
    fillRect(0, 0, W, H, Col{0, 0, 0, 175});
    int bw = 700, bh = 230, bx = (W - bw) / 2, by = (H - bh) / 2;
    fillRound(bx, by, bw, bh, 18, C_PANEL);
    drawText(fL, g_pTitle, bx + 30, by + 24, C_TXT);
    drawText(fS, g_pName, bx + 30, by + 80, C_DIM, bw - 60);
    fillRound(bx + 30, by + 124, bw - 60, 16, 8, C_PAN2);
    if (g_pTotal > 0) {
        double f = (double)g_pDone / (double)g_pTotal;
        if (f > 1) f = 1;
        int fw = (int)((bw - 60) * f);
        if (fw > 0) fillRound(bx + 30, by + 124, std::max(16, fw), 16, 8, C_GRN);
        char pc[16];
        snprintf(pc, sizeof pc, "%d%%", (int)(f * 100));
        drawText(fM, pc, bx + bw - 30, by + 150, C_TXT, 0, 1);
        drawText(fS, fmtSize(g_pDone) + " / " + fmtSize(g_pTotal), bx + 30, by + 156, C_DIM);
    }
    drawText(fS, "Segure B para cancelar", bx + bw / 2, by + 192, C_DIM, 0, 2);
}

static void tickProgress(bool force = false) {
    u64 now = armTicksToNs(armGetSystemTick()) / 1000000ULL;
    if (!force && now - g_lastUI < 80) return;
    g_lastUI = now;
    padUpdate(&pad);
    if (padGetButtons(&pad) & HidNpadButton_B) g_cancel = true;
    pump();
    drawMain();
    drawProgressOverlay();
    present();
}

// ---------------------------------------------------------------- operações

static std::string wrapText(const std::string& s, size_t width) {
    std::string out, line;
    size_t i = 0;
    while (i < s.size()) {
        size_t e = s.find(' ', i);
        if (e == std::string::npos) e = s.size();
        std::string w = s.substr(i, e - i);
        if (!line.empty() && line.size() + 1 + w.size() > width) {
            out += line + "\n";
            line.clear();
        }
        if (!line.empty()) line += " ";
        line += w;
        i = e + 1;
    }
    out += line;
    return out;
}

static void showBusy(const std::string& text) {
    drawMain();
    fillRect(0, 0, W, H, Col{0, 0, 0, 175});
    fillRound(W / 2 - 280, H / 2 - 56, 560, 112, 18, C_PANEL);
    drawText(fM, text, W / 2, H / 2 - 18, C_TXT, 520, 2);
    present();
}

static std::string safeName(std::string n) {
    for (auto& c : n)
        if (strchr("\\/:*?\"<>|", c) || (unsigned char)c < 32) c = '_';
    while (!n.empty() && (n.back() == '.' || n.back() == ' ')) n.pop_back();
    while (n.size() > 200) n.pop_back();
    while (!n.empty() && (((unsigned char)n.back() & 0xC0) == 0x80 || ((unsigned char)n.back() & 0xC0) == 0xC0)) n.pop_back();
    return n.empty() ? std::string("sem_nome") : n;
}

static bool copyFile(const std::string& s, const std::string& d) {
    FILE* in = fopen(s.c_str(), "rb");
    if (!in) { g_err = "Não foi possível abrir: " + baseName(s); return false; }
    FILE* out = fopen(d.c_str(), "wb");
    if (!out) { fclose(in); g_err = "Não foi possível criar: " + baseName(d); return false; }
    const size_t BS = 1 << 20;
    char* buf = (char*)malloc(BS);
    if (!buf) { fclose(in); fclose(out); g_err = "Memória insuficiente."; return false; }
    bool ok = true;
    size_t n;
    while ((n = fread(buf, 1, BS, in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) { ok = false; g_err = "Falha ao gravar (cartão cheio?)."; break; }
        g_pDone += n;
        tickProgress();
        if (g_cancel) { ok = false; break; }
    }
    if (ferror(in)) { ok = false; g_err = "Falha ao ler: " + baseName(s); }
    free(buf);
    fclose(in);
    fclose(out);
    if (!ok) remove(d.c_str());
    return ok;
}

static bool copyTree(const std::string& s, const std::string& d) {
    if (g_cancel) return false;
    g_pName = baseName(s);
    if (isDir(s)) {
        mkdir(d.c_str(), 0777);
        for (auto& n : listNames(s))
            if (!copyTree(join(s, n), join(d, n))) return false;
        return true;
    }
    return copyFile(s, d);
}

static bool removeTree(const std::string& p) {
    if (isDir(p)) {
        for (auto& n : listNames(p)) removeTree(join(p, n));
        g_pName = baseName(p);
        tickProgress();
        return rmdir(p.c_str()) == 0;
    }
    g_pName = baseName(p);
    tickProgress();
    return remove(p.c_str()) == 0;
}

static bool askText(const char* guide, const std::string& init, std::string& out) {
    SwkbdConfig k;
    if (R_FAILED(swkbdCreate(&k, 0))) return false;
    swkbdConfigMakePresetDefault(&k);
    swkbdConfigSetGuideText(&k, guide);
    swkbdConfigSetInitialText(&k, init.c_str());
    swkbdConfigSetStringLenMax(&k, 200);
    char buf[256] = {0};
    Result rc = swkbdShow(&k, buf, sizeof buf);
    swkbdClose(&k);
    if (R_FAILED(rc)) return false;
    out = buf;
    return !out.empty();
}

static void reloadBoth() {
    for (int i = 0; i < 2; i++) panes[i].reload();
    refreshSpace();
}

// ---------------------------------------------------------------- Google Drive

struct Bridge : gd::Progress {
    bool tick(uint64_t n) override {
        g_pDone += n;
        tickProgress();
        return !g_cancel;
    }
};
static Bridge g_bridge;
static int g_skipped = 0;

static bool driveLoad(Pane& p) {
    showBusy("Carregando o Google Drive...");
    std::vector<gd::Item> v;
    if (!gd::listFolder(p.driveId(), v)) {
        std::string e = gd::lastError();
        p.items.clear();
        message("Google Drive", "Não foi possível abrir a pasta.\n" + wrapText(e, 52));
        if (!gd::isLoggedIn()) {
            // login expirou: volta o painel para o cartão SD
            p.drive = false;
            p.dstack.clear();
            p.path = "sdmc:/";
            p.cur = 0;
            p.scroll = 0;
            p.reload();
        }
        return false;
    }
    for (auto& it : v) {
        Entry e;
        e.name = it.name;
        e.id = it.id;
        e.key = it.id;
        e.dir = it.dir;
        e.gdoc = it.gdoc;
        e.size = it.size;
        p.items.push_back(e);
    }
    return true;
}

static bool ensureDriveLogin() {
    if (!gd::netInit()) {
        message("Sem rede", wrapText(gd::lastError(), 52));
        return false;
    }
    if (gd::isLoggedIn()) return true;
    showBusy("Conectando ao Google Drive...");
    if (gd::loadSavedLogin()) return true;

    std::string url;
    if (!gd::loginServerStart(url)) {
        message("Erro", wrapText(gd::lastError(), 52));
        return false;
    }
    bool ok = false;
    std::string lastErr;
    for (;;) {
        if (!appletMainLoop()) { g_quit = true; break; }
        padUpdate(&pad);
        updateTouch();
        pump();
        if (padGetButtonsDown(&pad) & HidNpadButton_B) break;
        int bw = 900, bh = 460, bx = (W - bw) / 2, by = (H - bh) / 2;
        if (tp.released && !tp.moved && inRect(tp.x, tp.y, bx + bw / 2 - 150, by + bh - 76, 300, 56)) break;

        int r = gd::loginServerPoll();
        if (r == 1) { ok = true; break; }
        if (r == -1) lastErr = gd::lastError();

        drawMain();
        fillRect(0, 0, W, H, Col{0, 0, 0, 190});
        fillRound(bx, by, bw, bh, 20, C_PANEL);
        drawText(fL, "Entrar no Google Drive", bx + 36, by + 26, C_TXT);
        drawText(fM, "1. No celular (no mesmo Wi-Fi), abra o navegador e acesse:", bx + 36, by + 96, C_DIM, bw - 72);
        fillRound(bx + 36, by + 138, bw - 72, 78, 14, C_PAN2);
        drawText(fL, url, bx + bw / 2, by + 154, C_ACC, bw - 100, 2);
        drawText(fM, "2. Siga os passos que aparecem na página.", bx + 36, by + 240, C_DIM, bw - 72);
        drawText(fM, "3. Quando terminar, esta tela fecha sozinha.", bx + 36, by + 282, C_DIM, bw - 72);
        if (!lastErr.empty()) drawText(fS, fitText(fS, "Último erro: " + lastErr, bw - 72), bx + 36, by + 332, C_RED);
        fillRound(bx + bw / 2 - 150, by + bh - 76, 300, 56, 14, C_PAN2);
        drawText(fM, "B  Cancelar", bx + bw / 2, by + bh - 66, C_TXT, 0, 2);
        present();
    }
    gd::loginServerStop();
    return ok;
}

static void toggleDrive() {
    Pane& p = panes[active];
    p.filter.clear();
    if (p.drive) {
        p.drive = false;
        p.dstack.clear();
        p.path = "sdmc:/";
        p.cur = 0;
        p.scroll = 0;
        p.reload();
        return;
    }
    if (!ensureDriveLogin()) return;
    p.drive = true;
    p.dstack.clear();
    p.setDrivePath();
    p.cur = 0;
    p.scroll = 0;
    p.reload();
}

static u64 driveTreeSize(const Entry& e, int depth) {
    if (e.gdoc) return 0;
    if (!e.dir) return e.size;
    if (depth > 20) return 0;
    std::vector<gd::Item> v;
    if (!gd::listFolder(e.id, v)) return 0;
    u64 t = 0;
    for (auto& it : v) {
        Entry c;
        c.name = it.name;
        c.id = it.id;
        c.dir = it.dir;
        c.gdoc = it.gdoc;
        c.size = it.size;
        t += driveTreeSize(c, depth + 1);
    }
    return t;
}

// baixa um item (arquivo ou pasta) do Drive para dentro de dstDir
static bool dlTree(const Entry& e, const std::string& dstDir, std::string& outPath, int depth) {
    if (g_cancel) return false;
    if (e.gdoc) { g_skipped++; return true; }
    g_pName = e.name;
    std::string dst = uniqueName(join(dstDir, safeName(e.name)));
    if (depth == 0) outPath = dst;
    if (e.dir) {
        if (depth > 20) return true;
        mkdir(dst.c_str(), 0777);
        std::vector<gd::Item> v;
        if (!gd::listFolder(e.id, v)) { g_err = gd::lastError(); return false; }
        for (auto& it : v) {
            Entry c;
            c.name = it.name;
            c.id = it.id;
            c.dir = it.dir;
            c.gdoc = it.gdoc;
            c.size = it.size;
            std::string tmp;
            if (!dlTree(c, dst, tmp, depth + 1)) return false;
        }
        return true;
    }
    gd::Item it;
    it.id = e.id;
    it.name = e.name;
    it.size = e.size;
    if (!gd::downloadFile(it, dst, &g_bridge)) { g_err = gd::lastError(); return false; }
    return true;
}

// envia um item (arquivo ou pasta) do cartão SD para a pasta parentId do Drive
static bool ulTree(const std::string& src, const std::string& parentId) {
    if (g_cancel) return false;
    g_pName = baseName(src);
    if (isDir(src)) {
        std::string nid;
        if (!gd::createFolder(parentId, baseName(src), nid)) { g_err = gd::lastError(); return false; }
        for (auto& n : listNames(src))
            if (!ulTree(join(src, n), nid)) return false;
        return true;
    }
    if (!gd::uploadFile(src, parentId, baseName(src), &g_bridge)) { g_err = gd::lastError(); return false; }
    return true;
}

// ---------------------------------------------------------------- ZIP

struct ZBridge : zx::Progress {
    bool tick(uint64_t n, const char* name) override {
        g_pDone += n;
        g_pName = name ? name : "";
        tickProgress();
        return !g_cancel;
    }
};
static ZBridge g_zbridge;

static bool endsWithZip(const std::string& n) {
    return n.size() > 4 && lower(n.substr(n.size() - 4)) == ".zip";
}

static std::string stemOf(const std::string& n) {
    std::string t = n;
    size_t d = t.find_last_of('.');
    if (d != std::string::npos && d > 0) t = t.substr(0, d);
    return t;
}

static void focusOn(const std::string& parent, const std::string& name) {
    for (int i = 0; i < 2; i++)
        if (!panes[i].drive && panes[i].path == parent) panes[i].reload(name);
}

static void doExtractZip(const std::string& zipPath, const std::string& destParent) {
    zx::Info info;
    if (!zx::inspect(zipPath, info)) { message("ZIP", wrapText(zx::lastError(), 52)); return; }
    if (info.encrypted &&
        modal("ZIP com senha", "Alguns arquivos têm senha e não podem ser extraídos.\nOs demais serão extraídos normalmente.", {"Cancelar", "Continuar"}, 1) != 1)
        return;
    std::string dest = uniqueName(join(destParent, safeName(stemOf(baseName(zipPath)))));
    g_cancel = false;
    g_pTitle = "Extraindo...";
    g_pDone = 0;
    g_pTotal = info.totalBytes;
    g_pName = baseName(zipPath);
    tickProgress(true);
    int skipped = 0;
    bool ok = zx::extract(zipPath, dest, &g_zbridge, skipped);
    std::string err = zx::lastError();
    if (!ok) removeTree(dest);
    reloadBoth();
    if (ok) focusOn(destParent, baseName(dest));
    if (!ok) message(g_cancel ? "Cancelado" : "Erro ao extrair", wrapText(g_cancel ? std::string("A extração foi cancelada.") : err, 52));
    else if (skipped > 0)
        message("Concluído", std::to_string(skipped) + " item(ns) foram ignorados\n(caminho inseguro, senha ou formato não suportado).");
}

static void doCompressZip(int level) {
    Pane& p = panes[active];
    if (p.drive) { message("ZIP", "Compactar só funciona com arquivos do cartão SD.\nBaixe os arquivos primeiro."); return; }
    auto sel = selection(p);
    if (sel.empty()) return;
    std::vector<std::string> paths;
    for (auto& e : sel) paths.push_back(join(p.path, e.name));
    std::string base = sel.size() == 1 ? sel[0].name : std::string("Arquivos");
    if (sel.size() == 1 && !sel[0].dir) base = stemOf(base);
    std::string zipPath = uniqueName(join(p.path, safeName(base) + ".zip"));
    g_cancel = false;
    g_pTitle = "Compactando...";
    g_pDone = 0;
    g_pTotal = 0;
    for (auto& e : sel) g_pTotal += treeSize(join(p.path, e.name));
    g_pName = baseName(zipPath);
    tickProgress(true);
    bool ok = zx::compress(paths, zipPath, level, &g_zbridge);
    std::string err = zx::lastError();
    reloadBoth();
    if (ok) focusOn(p.path, baseName(zipPath));
    else message(g_cancel ? "Cancelado" : "Erro ao compactar", wrapText(g_cancel ? std::string("A compactação foi cancelada.") : err, 52));
}

static void showZipInfo(const std::string& zipPath) {
    zx::Info info;
    if (!zx::inspect(zipPath, info)) { message("ZIP", wrapText(zx::lastError(), 52)); return; }
    message(baseName(zipPath), "Arquivos: " + std::to_string(info.files) + "\nPastas: " + std::to_string(info.dirs) +
                                  "\nTamanho descompactado: " + fmtSize(info.totalBytes) + (info.encrypted ? "\nContém itens com senha." : ""));
}

static void zipMenu() {
    Pane& p = panes[active];
    Pane& o = panes[1 - active];
    std::vector<std::string> opts = {"Compactar a seleção (rápido)", "Compactar a seleção (sem compressão)",
                                     "Extrair o ZIP selecionado aqui", "Extrair o ZIP selecionado no outro painel", "Informações do ZIP"};
    int r = modal("ZIP", "", opts);
    if (r < 0) return;
    if (r <= 1) { doCompressZip(r == 0 ? 1 : 0); return; }
    if (p.drive) { message("ZIP", "Extrair só funciona com arquivos do cartão SD."); return; }
    if (p.items.empty() || p.items[p.cur].dir || !endsWithZip(p.items[p.cur].name)) {
        message("ZIP", "Selecione um arquivo .zip na lista primeiro.");
        return;
    }
    std::string zipPath = join(p.path, p.items[p.cur].name);
    if (r == 2) doExtractZip(zipPath, p.path);
    else if (r == 3) {
        if (o.drive) { message("ZIP", "O outro painel precisa ser uma pasta do cartão SD."); return; }
        doExtractZip(zipPath, o.path);
    } else showZipInfo(zipPath);
}

// ---------------------------------------------------------------- propriedades e espaço

struct TreeInfo {
    u64 bytes = 0;
    u64 files = 0;
    u64 dirs = 0;
};

static bool calcTree(const std::string& p, TreeInfo& t, int depth) {
    if (g_cancel) return false;
    struct stat st;
    if (stat(p.c_str(), &st) != 0) return true;
    if (!S_ISDIR(st.st_mode)) {
        t.bytes += (u64)st.st_size;
        t.files++;
        g_pName = baseName(p);
        tickProgress();
        return !g_cancel;
    }
    t.dirs++;
    if (depth > 64) return true;
    for (auto& n : listNames(p))
        if (!calcTree(join(p, n), t, depth + 1)) return false;
    return true;
}

static std::string fmtDate(time_t tt) {
    if (tt <= 0) return "-";
    struct tm* lt = localtime(&tt);
    if (!lt) return "-";
    char b[32];
    strftime(b, sizeof b, "%d/%m/%Y %H:%M", lt);
    return b;
}

// texto da janela de propriedades (separado para facilitar testes)
static std::string propsText(const std::vector<Entry>& sel, const TreeInfo& t, const std::string& where) {
    int selDirs = 0;
    for (auto& e : sel) if (e.dir) selDirs++;
    std::string r;
    if (sel.size() == 1) r += std::string("Tipo: ") + (sel[0].dir ? "Pasta" : "Arquivo") + "\n";
    else r += "Itens selecionados: " + std::to_string(sel.size()) + "\n";
    r += "Tamanho: " + fmtSize(t.bytes);
    if (t.bytes >= 1024) r += " (" + std::to_string((unsigned long long)t.bytes) + " bytes)";
    r += "\n";
    if (selDirs > 0 || sel.size() > 1)
        r += "Conteúdo: " + std::to_string((unsigned long long)t.files) + " arquivo(s) e " +
             std::to_string((unsigned long long)(t.dirs >= (u64)selDirs ? t.dirs - selDirs : 0)) + " pasta(s)\n";
    if (sel.size() == 1) r += "Modificado: " + fmtDate(sel[0].mtime) + "\n";
    r += "Local: " + where;
    return r;
}

static void doProperties() {
    Pane& p = panes[active];
    auto sel = selection(p);
    if (sel.empty()) return;
    std::string title = sel.size() == 1 ? sel[0].name : std::to_string(sel.size()) + " itens";
    if (p.drive) {
        if (sel.size() == 1 && !sel[0].dir)
            message(title, std::string("Tipo: Arquivo") + (sel[0].gdoc ? " nativo do Google" : "") + "\nTamanho: " + fmtSize(sel[0].size) + "\nLocal: " + p.path);
        else
            message(title, "O tamanho de pastas do Google Drive não é calculado aqui.\nLocal: " + p.path);
        return;
    }
    g_cancel = false;
    g_pTitle = "Calculando...";
    g_pDone = 0;
    g_pTotal = 0;
    g_pName = title;
    tickProgress(true);
    TreeInfo t;
    for (auto& e : sel) {
        if (!calcTree(join(p.path, e.name), t, 0)) {
            message("Cancelado", "O cálculo foi cancelado.");
            return;
        }
    }
    message(title, propsText(sel, t, p.path));
}

static void doSpaceAnalyzer() {
    Pane& p = panes[active];
    if (p.drive) { message("Espaço ocupado", "O analisador funciona somente no cartão SD."); return; }
    g_cancel = false;
    g_pTitle = "Analisando o espaço...";
    g_pDone = 0;
    g_pTotal = 0;
    g_pName = p.path;
    tickProgress(true);
    std::vector<std::pair<u64, std::string>> v;
    u64 total = 0;
    for (auto& n : listNames(p.path)) {
        TreeInfo t;
        if (!calcTree(join(p.path, n), t, 0)) {
            message("Cancelado", "A análise foi cancelada.");
            return;
        }
        v.push_back(std::make_pair(t.bytes, n));
        total += t.bytes;
    }
    if (v.empty()) { message("Espaço ocupado", "Esta pasta está vazia."); return; }
    std::sort(v.begin(), v.end(), [](const std::pair<u64, std::string>& a, const std::pair<u64, std::string>& b) { return a.first > b.first; });
    std::vector<std::string> opts;
    size_t shown = std::min<size_t>(v.size(), 9);
    for (size_t i = 0; i < shown; i++) {
        int pct = total > 0 ? (int)(v[i].first * 100 / total) : 0;
        opts.push_back(fmtSize(v[i].first) + " (" + std::to_string(pct) + "%)  " + v[i].second);
    }
    int r = modal("Maiores itens", "Total desta pasta: " + fmtSize(total) + "\nToque em um item para ir até ele.", opts);
    if (r >= 0 && r < (int)shown) {
        p.filter.clear();
        p.reload(v[r].second);
    }
}

static void propsMenu() {
    int r = modal("Propriedades e espaço", "", {"Propriedades do item (ou da seleção)", "Espaço ocupado (maiores itens da pasta)"});
    if (r == 0) doProperties();
    else if (r == 1) doSpaceAnalyzer();
}

// ---------------------------------------------------------------- ações

static void doCopyMove(bool move) {
    Pane& s = panes[active];
    Pane& d = panes[1 - active];
    auto sel = selection(s);
    if (sel.empty()) return;
    if (s.drive && d.drive) {
        message("Aviso", "Copiar de uma pasta do Drive para outra ainda não é suportado.\nUse o outro painel para o cartão SD.");
        return;
    }
    if (!s.drive && !d.drive && move && s.path == d.path) {
        message("Aviso", "Origem e destino são a mesma pasta.\nAbra outra pasta no painel ao lado.");
        return;
    }
    g_cancel = false;
    g_err.clear();
    g_skipped = 0;
    g_pTitle = move ? "Movendo..." : (s.drive ? "Baixando..." : (d.drive ? "Enviando..." : "Copiando..."));
    g_pDone = 0;
    g_pTotal = 0;
    if (s.drive) {
        showBusy("Calculando o tamanho...");
        for (auto& e : sel) g_pTotal += driveTreeSize(e, 0);
    } else {
        for (auto& e : sel) g_pTotal += treeSize(join(s.path, e.name));
    }
    tickProgress(true);

    int fail = 0;
    for (auto& e : sel) {
        if (g_cancel) break;
        bool ok = true;
        if (s.drive) {
            std::string outPath;
            ok = dlTree(e, d.path, outPath, 0);
            if (!ok && !outPath.empty()) removeTree(outPath);
            if (ok && move && !e.gdoc && !gd::trashItem(e.id)) { ok = false; g_err = gd::lastError(); }
        } else if (d.drive) {
            std::string from = join(s.path, e.name);
            ok = ulTree(from, d.driveId());
            if (ok && move) ok = removeTree(from);
        } else {
            std::string from = join(s.path, e.name);
            std::string to = uniqueName(join(d.path, e.name));
            if (e.dir && (to == from || to.compare(0, from.size() + 1, from + "/") == 0)) {
                fail++;
                g_err = "Não é possível copiar uma pasta para dentro dela mesma.";
                continue;
            }
            if (move && rename(from.c_str(), to.c_str()) == 0) {
                ok = true;
            } else {
                ok = copyTree(from, to);
                if (!ok) removeTree(to);
                else if (move) ok = removeTree(from);
            }
        }
        if (!ok) fail++;
    }
    reloadBoth();
    if (g_cancel) message("Cancelado", "A operação foi cancelada.");
    else if (fail > 0) message("Atenção", wrapText(g_err.empty() ? std::string("Alguns itens falharam.") : g_err, 52));
    else if (g_skipped > 0)
        message("Concluído", std::to_string(g_skipped) + " arquivo(s) nativos do Google (Docs/Planilhas)\nforam ignorados: não podem ser baixados.");
}

static void doDelete() {
    Pane& p = panes[active];
    auto sel = selection(p);
    if (sel.empty()) return;
    std::string what = sel.size() == 1 ? "\"" + sel[0].name + "\"" : std::to_string(sel.size()) + " itens";
    if (p.drive) {
        if (modal("Apagar", "Mover " + what + " para a lixeira do Google Drive?\nVocê pode restaurar depois pelo próprio Drive.", {"Cancelar", "Apagar (lixeira)"}, 0) != 1) return;
        showBusy("Apagando...");
        int fail = 0;
        for (auto& e : sel) if (!gd::trashItem(e.id)) fail++;
        std::string err = gd::lastError();
        reloadBoth();
        if (fail) message("Atenção", wrapText(err, 52));
        return;
    }
    if (modal("Apagar", "Apagar " + what + "?\nEsta ação não pode ser desfeita.", {"Cancelar", "Apagar"}, 0) != 1) return;
    for (auto& e : sel) {
        if (isCritical(join(p.path, e.name))) {
            if (modal("Pasta importante", "\"" + e.name + "\" é usada pelo seu sistema/CFW.\nApagar pode causar problemas.", {"Cancelar", "Apagar mesmo assim"}, 0) != 1) return;
        }
    }
    g_cancel = false;
    g_pTitle = "Apagando...";
    g_pDone = 0;
    g_pTotal = 0;
    for (auto& e : sel) removeTree(join(p.path, e.name));
    reloadBoth();
}

static bool validName(const std::string& n) {
    if (n.empty() || n == "." || n == "..") return false;
    return n.find_first_of("/\\:*?\"<>|") == std::string::npos;
}

static void doRename() {
    Pane& p = panes[active];
    if (p.items.empty()) return;
    Entry cur = p.items[p.cur];
    std::string nn;
    if (!askText("Novo nome", cur.name, nn)) return;
    if (!validName(nn)) { message("Nome inválido", "O nome contém caracteres não permitidos."); return; }
    if (nn == cur.name) return;
    if (p.drive) {
        showBusy("Renomeando...");
        if (!gd::renameItem(cur.id, nn)) { message("Erro", wrapText(gd::lastError(), 52)); return; }
    } else {
        if (pathExists(join(p.path, nn))) { message("Já existe", "Já existe um item com esse nome."); return; }
        if (rename(join(p.path, cur.name).c_str(), join(p.path, nn).c_str()) != 0) {
            message("Erro", "Não foi possível renomear.");
            return;
        }
    }
    p.reload(nn);
    if (!panes[1 - active].drive && !p.drive && panes[1 - active].path == p.path) panes[1 - active].reload();
}

static void doMkdir() {
    Pane& p = panes[active];
    std::string nn;
    if (!askText("Nome da nova pasta", "", nn)) return;
    if (!validName(nn)) { message("Nome inválido", "O nome contém caracteres não permitidos."); return; }
    if (p.drive) {
        showBusy("Criando a pasta...");
        std::string nid;
        if (!gd::createFolder(p.driveId(), nn, nid)) { message("Erro", wrapText(gd::lastError(), 52)); return; }
    } else if (mkdir(join(p.path, nn).c_str(), 0777) != 0) {
        message("Erro", "Não foi possível criar a pasta.");
        return;
    }
    p.reload(nn);
    if (!panes[1 - active].drive && !p.drive && panes[1 - active].path == p.path) panes[1 - active].reload();
}

static bool endsWithNro(const std::string& n) {
    return n.size() > 4 && lower(n.substr(n.size() - 4)) == ".nro";
}

// Fecha o Switch File Manager e abre outro programa (.nro) pelo Homebrew Menu
static void launchNro(const std::string& path) {
    if (!envHasNextLoad()) {
        message("Não disponível", "Este modo de execução não permite abrir outros programas.\nAbra o Switch File Manager pelo Homebrew Menu.");
        return;
    }
    std::string argv = "\"" + path + "\"";
    Result rc = envSetNextLoad(path.c_str(), argv.c_str());
    if (R_FAILED(rc)) {
        char b[80];
        snprintf(b, sizeof b, "Código do erro: 2%03d-%04d", (int)R_MODULE(rc), (int)R_DESCRIPTION(rc));
        message("Não foi possível abrir", b);
        return;
    }
    g_quit = true;
}

static void openCur() {
    Pane& p = panes[active];
    if (p.items.empty()) return;
    Entry e = p.items[p.cur];
    if (e.dir) {
        if (p.drive) {
            p.dstack.push_back(std::make_pair(e.id, e.name));
            p.setDrivePath();
        } else {
            p.path = join(p.path, e.name);
        }
        p.cur = 0;
        p.scroll = 0;
        p.filter.clear();
        p.reload();
    } else if (!p.drive && endsWithZip(e.name)) {
        int r = modal("Arquivo ZIP", "\"" + e.name + "\"", {"Cancelar", "Extrair aqui", "Extrair no outro painel", "Informações"}, 1);
        std::string zp = join(p.path, e.name);
        if (r == 1) doExtractZip(zp, p.path);
        else if (r == 2) {
            if (panes[1 - active].drive) message("ZIP", "O outro painel precisa ser uma pasta do cartão SD.");
            else doExtractZip(zp, panes[1 - active].path);
        } else if (r == 3) showZipInfo(zp);
    } else if (!p.drive && endsWithNro(e.name)) {
        if (modal("Abrir programa", "Abrir \"" + e.name + "\" agora?\nO Switch File Manager será fechado.", {"Cancelar", "Abrir"}, 1) == 1)
            launchNro(join(p.path, e.name));
    } else if (e.gdoc) {
        message(e.name, "Arquivo nativo do Google (Docs/Planilhas/Slides).\nEle não pode ser baixado por aqui.");
    } else {
        message(e.name, "Tamanho: " + fmtSize(e.size) + "\nPasta: " + p.path);
    }
}

static void goUp() {
    Pane& p = panes[active];
    if (p.drive) {
        if (p.dstack.empty()) return;
        std::string was = p.dstack.back().second;
        p.dstack.pop_back();
        p.setDrivePath();
        p.filter.clear();
        p.reload(was);
        return;
    }
    if (p.path == "sdmc:/") return;
    std::string was = baseName(p.path);
    p.path = parentOf(p.path);
    p.filter.clear();
    p.reload(was);
}

static void toggleMark() {
    Pane& p = panes[active];
    if (p.items.empty()) return;
    const std::string& k = p.items[p.cur].key;
    if (p.marks.count(k)) p.marks.erase(k);
    else p.marks.insert(k);
    if (p.cur + 1 < (int)p.items.size()) p.cur++;
    p.ensureVisible();
}

static void logoutAll() {
    gd::logout();
    for (int i = 0; i < 2; i++) {
        if (panes[i].drive) {
            panes[i].drive = false;
            panes[i].dstack.clear();
            panes[i].path = "sdmc:/";
            panes[i].cur = 0;
            panes[i].scroll = 0;
            panes[i].reload();
        }
    }
}

static void cloudMenu() {
    Pane& p = panes[active];
    std::vector<std::string> o = {
        p.drive ? "Voltar este painel ao cartão SD" : "Abrir o Google Drive neste painel",
        "Trocar de conta do Google"};
    int r = modal("Google Drive", "", o);
    if (r == 0) toggleDrive();
    else if (r == 1) {
        if (modal("Trocar de conta", "Isso desconecta o Google Drive deste Switch.\nVocê entrará de novo na próxima vez.", {"Cancelar", "Desconectar"}, 0) == 1) logoutAll();
    }
}

static bool goToPath(const std::string& path) {
    if (!isDir(path)) {
        message("Pasta não encontrada", "Essa pasta não existe mais:\n" + path);
        return false;
    }
    Pane& p = panes[active];
    p.drive = false;
    p.dstack.clear();
    p.path = path;
    p.cur = 0;
    p.scroll = 0;
    p.filter.clear();
    p.reload();
    return true;
}

static void favoritesList() {
    for (;;) {
        if (g_favs.empty()) {
            message("Favoritos", "Você ainda não tem favoritos.\nAbra uma pasta e use \"Adicionar esta pasta\".");
            return;
        }
        int r = modal("Favoritos", "Toque em uma pasta para escolher o que fazer.", g_favs);
        if (r < 0) return;
        int a = modal(baseName(g_favs[r]), g_favs[r], {"Ir para a pasta", "Remover dos favoritos", "Cancelar"}, 0);
        if (a == 0) { goToPath(g_favs[r]); return; }
        if (a == 1) {
            g_favs.erase(g_favs.begin() + r);
            saveLines("favoritos.txt", g_favs);
        }
    }
}

static void historyList() {
    if (g_hist.empty()) { message("Pastas recentes", "Ainda não há pastas recentes."); return; }
    std::vector<std::string> o = g_hist;
    o.push_back("Limpar o histórico");
    int r = modal("Pastas recentes", "Toque em uma pasta para ir até ela.", o);
    if (r < 0) return;
    if (r == (int)g_hist.size()) {
        if (modal("Limpar o histórico", "Apagar a lista de pastas recentes?", {"Cancelar", "Limpar"}, 0) == 1) {
            g_hist.clear();
            saveLines("historico.txt", g_hist);
        }
        return;
    }
    goToPath(g_hist[r]);
}

static void favMenu() {
    Pane& p = panes[active];
    std::vector<std::string> o = {"Adicionar esta pasta aos favoritos", "Ver favoritos (" + std::to_string(g_favs.size()) + ")",
                                  "Pastas recentes (" + std::to_string(g_hist.size()) + ")"};
    int r = modal("Favoritos e histórico", p.drive ? std::string() : "Pasta atual: " + p.path, o);
    if (r == 0) {
        if (p.drive) { message("Favoritos", "Só pastas do cartão SD podem ser favoritas."); return; }
        if (std::find(g_favs.begin(), g_favs.end(), p.path) != g_favs.end()) {
            message("Favoritos", "Essa pasta já está nos favoritos.");
            return;
        }
        g_favs.push_back(p.path);
        saveLines("favoritos.txt", g_favs);
        message("Favoritos", "Pasta adicionada aos favoritos.");
    } else if (r == 1) favoritesList();
    else if (r == 2) historyList();
}

static void sortMenu() {
    Pane& p = panes[active];
    std::vector<std::string> o = {"Ordenar por nome", "Ordenar por tamanho", "Ordenar por data", "Ordenar por tipo",
                                  "Inverter a ordem", "Buscar por nome...", "Limpar a busca", "Atualizar as listas"};
    int r = modal("Ordenar e buscar", "Ordem atual: " + p.sortLabel() + (p.filter.empty() ? "" : "\nBusca: " + p.filter), o);
    if (r >= 0 && r <= 3) {
        p.sortMode = r;
        p.sortDesc = (r == 1 || r == 2);  // tamanho e data: maiores/mais novos primeiro
        p.reload();
    } else if (r == 4) {
        p.sortDesc = !p.sortDesc;
        p.reload();
    } else if (r == 5) {
        std::string q;
        if (askText("Buscar por nome", p.filter, q)) {
            p.filter = q;
            p.cur = 0;
            p.scroll = 0;
            p.reload();
        }
    } else if (r == 6) {
        p.filter.clear();
        p.reload();
    } else if (r == 7) {
        reloadBoth();
    }
}

// Seleção em lote no painel ativo. Respeita a busca atual e não altera arquivos.
static void selectionMenu() {
    Pane& p = panes[active];
    const std::vector<std::string> opts = {
        "Marcar todos os itens visíveis",
        "Marcar somente arquivos",
        "Marcar somente pastas",
        "Inverter marcação visível",
        "Limpar todas as marcações"
    };
    int choice = modal("Seleção múltipla",
        std::to_string(p.marks.size()) + " marcado(s) / " +
        std::to_string(p.items.size()) + " item(ns) visíveis", opts);
    if (choice < 0) return;
    if (choice == 4) { p.marks.clear(); return; }
    for (const auto& e : p.items) {
        if (choice == 0 || (choice == 1 && !e.dir) || (choice == 2 && e.dir))
            p.marks.insert(e.key);
        else if (choice == 3) {
            if (p.marks.count(e.key)) p.marks.erase(e.key);
            else p.marks.insert(e.key);
        }
    }
}

static void actionsMenu() {
    Pane& p = panes[active];
    std::vector<std::string> o = {
        "Copiar para o outro painel", "Mover para o outro painel", "Renomear",
        "Nova pasta", "Apagar", "Seleção múltipla...",
        "Ordenar e buscar...", "Compactar e extrair (ZIP)...", "Propriedades e espaço...", "Favoritos e histórico...", "Google Drive e conta..."};
    int r = modal("Ações", "", o);
    switch (r) {
        case 0: doCopyMove(false); break;
        case 1: doCopyMove(true); break;
        case 2: doRename(); break;
        case 3: doMkdir(); break;
        case 4: doDelete(); break;
        case 5: selectionMenu(); break;
        case 6: sortMenu(); break;
        case 7: zipMenu(); break;
        case 8: propsMenu(); break;
        case 9: favMenu(); break;
        case 10: cloudMenu(); break;
        default: break;
    }
}

static void dispatch(int id) {
    switch (id) {
        case B_OPEN: openCur(); break;
        case B_UP: goUp(); break;
        case B_MARK: toggleMark(); break;
        case B_ACT: actionsMenu(); break;
        case B_PANE: active = 1 - active; break;
        case B_CLOUD: toggleDrive(); break;
        case B_EXIT: g_quit = true; break;
    }
}

static void movePane(Pane& p, int delta) {
    if (p.items.empty()) return;
    p.cur = std::max(0, std::min((int)p.items.size() - 1, p.cur + delta));
    p.ensureVisible();
}

static void handleInput() {
    static int holdUp = 0, holdDown = 0, dragPane = -1, accum = 0;
    u64 d = padGetButtonsDown(&pad);
    u64 h = padGetButtons(&pad);

    if (d & HidNpadButton_Plus) { g_quit = true; return; }
    if (d & HidNpadButton_A) { dispatch(B_OPEN); return; }
    if (d & HidNpadButton_B) { dispatch(B_UP); return; }
    if (d & HidNpadButton_Y) { dispatch(B_MARK); return; }
    if (d & HidNpadButton_X) { dispatch(B_ACT); return; }
    if (d & (HidNpadButton_L | HidNpadButton_R)) { dispatch(B_PANE); return; }
    if (d & HidNpadButton_Minus) { dispatch(B_CLOUD); return; }
    if (d & HidNpadButton_Left) active = 0;
    if (d & HidNpadButton_Right) active = 1;
    if (d & HidNpadButton_ZL) movePane(panes[active], -ROWS);
    if (d & HidNpadButton_ZR) movePane(panes[active], ROWS);

    if (h & (HidNpadButton_Up | HidNpadButton_StickLUp)) {
        holdUp++;
        if (holdUp == 1 || (holdUp > 18 && holdUp % 4 == 0)) movePane(panes[active], -1);
    } else holdUp = 0;
    if (h & (HidNpadButton_Down | HidNpadButton_StickLDown)) {
        holdDown++;
        if (holdDown == 1 || (holdDown > 18 && holdDown % 4 == 0)) movePane(panes[active], 1);
    } else holdDown = 0;

    // toque
    if (tp.pressed) {
        dragPane = (tp.y >= PANE_Y && tp.y < PANE_Y + PANE_H) ? (tp.x < 640 ? 0 : 1) : -1;
        accum = 0;
    }
    if (tp.down && tp.moved && dragPane >= 0) {
        accum += tp.ly - tp.y;
        tp.ly = tp.y;
        Pane& p = panes[dragPane];
        while (accum >= ROW_H) { p.scroll++; accum -= ROW_H; }
        while (accum <= -ROW_H) { p.scroll--; accum += ROW_H; }
        p.clamp();
    }
    if (tp.released && !tp.moved) {
        for (int i = 0; i < NBTN; i++) {
            if (inRect(tp.x, tp.y, 22 + i * 178, 646, 168, 58)) { dispatch(BTNS[i].id); return; }
        }
        if (tp.y >= PANE_Y && tp.y < PANE_Y + PANE_H) {
            int pi = tp.x < 640 ? 0 : 1;
            Pane& p = panes[pi];
            if (tp.y >= LIST_TOP) {
                int idx = p.scroll + (tp.y - LIST_TOP) / ROW_H;
                if (idx >= 0 && idx < (int)p.items.size()) {
                    if (active == pi && p.cur == idx) { openCur(); return; }
                    active = pi;
                    p.cur = idx;
                } else active = pi;
            } else active = pi;
        }
    }
}

// ---------------------------------------------------------------- main

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    hidInitializeTouchScreen();
    plInitialize(PlServiceType_User);

    canvas = (uint32_t*)malloc((size_t)W * H * 4);
    framebufferCreate(&g_fb, nwindowGetDefault(), W, H, PIXEL_FORMAT_RGBA_8888, 2);
    framebufferMakeLinear(&g_fb);

    PlFontData font;
    if (canvas && R_SUCCEEDED(plGetSharedFontByType(&font, PlSharedFontType_Standard))) {
        const unsigned char* data = (const unsigned char*)font.address;
        int off = stbtt_GetFontOffsetForIndex(data, 0);
        if (stbtt_InitFont(&g_info, data, off)) {
            fS = makeFnt(21);
            fM = makeFnt(26);
            fL = makeFnt(34);
            g_fontOk = true;
        }
    }

    if (g_fontOk) {
        g_favs = loadLines("favoritos.txt");
        g_hist = loadLines("historico.txt");
        refreshSpace();
        panes[0].reload();
        panes[1].reload();
        int frame = 0;
        while (!g_quit && appletMainLoop()) {
            padUpdate(&pad);
            updateTouch();
            pump();
            handleInput();
            if (++frame % 300 == 0) refreshSpace();
            drawMain();
            present();
        }
    }

    gd::netExit();
    framebufferClose(&g_fb);
    free(canvas);
    plExit();
    return 0;
}
