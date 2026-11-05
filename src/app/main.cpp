// Switch File Manager - interface Commander, dois paineis e operacoes seguras
// C++ / libnx (renderização por software, sem bibliotecas externas) + Google Drive

#include <switch.h>
#include "version.h"
#include "sfm_diag.h" // SFM_DIAG_0107
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

#include "core/paths.h"
#include "storage/filesystem.h"
#include "storage/operations.h"
#include "security/paths.h"
#include "ui/renderer.h"
using namespace sfm::paths;
using namespace sfm::storage;
using namespace sfm::security;
using namespace sfm::ui;
#include "gdrive.h"
#include "zipx.h"
#include "wifi_transfer.h"

static PadState pad;
static bool g_quit = false, g_cancel = false;
static u64 g_free = 0, g_total = 0;

// ---------------------------------------------------------------- utilidades

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
static const char* CFG_DIR = sfm::paths::kLegacyConfigDir;
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
    if (wifi::running()) {
        std::string ftpStatus = "FTP ATIVO | " +
            std::to_string(wifi::activeClients()) + " clientes | " +
            std::to_string(wifi::directoryListings()) + " listagens | " +
            std::to_string(wifi::filesCompleted()) + " arquivos";
        drawText(fS, ftpStatus, 405, 45, C_GRN, 585);
    }

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

        // SFM_FTP_STATUS_0105: atualização visual em tempo real.
        if (title == "Servidor FTP ativo" && wifi::running()) {
            std::string last = wifi::lastActivity();
            if (last.size() > 62) last = last.substr(0, 59) + "...";
            lines = splitLines(
                "Endereco FTP:\n" + wifi::url() +
                "\nClientes conectados: " + std::to_string(wifi::activeClients()) +
                "\nArquivos concluidos: " + std::to_string(wifi::filesCompleted()) +
                "\nTotal transferido: " + fmtSize(wifi::bytesTransferred()) +
                "\nUltima atividade:\n" + last);
        }
        static unsigned lastModalFtpChanges = 0;
        unsigned modalFtpChanges = wifi::fileChanges();
        if (modalFtpChanges != lastModalFtpChanges) {
            lastModalFtpChanges = modalFtpChanges;
            for (int pi = 0; pi < 2; ++pi)
                if (!panes[pi].drive) panes[pi].reload();
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
static u64 g_pStartMs = 0;
static u64 g_pFilesDone = 0, g_pFilesTotal = 0;
static bool g_pCountFiles = false;
static u64 progressNowMs() { return armTicksToNs(armGetSystemTick()) / 1000000ULL; }
static std::string formatDuration(u64 seconds) {
    char b[32];
    if (seconds >= 3600) snprintf(b, sizeof b, "%lluh %02llum", (unsigned long long)(seconds / 3600), (unsigned long long)((seconds / 60) % 60));
    else snprintf(b, sizeof b, "%llum %02llus", (unsigned long long)(seconds / 60), (unsigned long long)(seconds % 60));
    return b;
}
static void beginTransferProgress() {
    g_pDone = 0; g_pTotal = 0; g_lastUI = 0;
    g_pFilesDone = 0; g_pFilesTotal = 0; g_pCountFiles = true;
    g_pStartMs = progressNowMs();
}
static void countLocalFiles(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return;
    if (!S_ISDIR(st.st_mode)) { ++g_pFilesTotal; return; }
    for (const auto& name : listNames(path)) countLocalFiles(join(path, name));
}


static void drawProgressOverlay() {
    fillRect(0, 0, W, H, Col{0, 0, 0, 175});
    int bw = 700, bh = 260, bx = (W - bw) / 2, by = (H - bh) / 2;
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
    if (g_pCountFiles) {
        std::string files = "Arquivos: " + std::to_string(g_pFilesDone) + " / " + std::to_string(g_pFilesTotal);
        drawText(fS, files, bx + 30, by + 184, C_DIM);
    }
    if (g_pStartMs > 0 && g_pDone > 0) {
        u64 elapsed = progressNowMs() - g_pStartMs;
        if (elapsed >= 1000) {
            double bytesPerSecond = (double)g_pDone * 1000.0 / (double)elapsed;
            std::string stats = "Velocidade media: " + fmtSize((u64)bytesPerSecond) + "/s";
            if (g_pTotal > g_pDone && bytesPerSecond >= 1024.0) {
                u64 eta = (u64)((double)(g_pTotal - g_pDone) / bytesPerSecond);
                stats += "  |  Restante estimado: " + formatDuration(eta);
            }
            drawText(fS, stats, bx + 30, by + 208, C_DIM, bw - 60);
        }
    }
    drawText(fS, "B: solicitar cancelamento", bx + bw - 30, by + 184, C_DIM, 0, 1);
}

static void tickProgress(bool force = false) {
    u64 now = progressNowMs();
    // Capturar B em toda chamada, mesmo quando o desenho da tela é limitado a 80 ms.
    padUpdate(&pad);
    if (padGetButtonsDown(&pad) & HidNpadButton_B) g_cancel = true;
    if (!force && now - g_lastUI < 80) return;
    g_lastUI = now;
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

struct LocalProgress : sfm::storage::Progress {
    bool cancelled() const override { return g_cancel; }
    void update(const std::string& name, uint64_t bytes, bool completed) override {
        g_pName = name; g_pDone += bytes;
        if (completed && g_pCountFiles) ++g_pFilesDone;
        tickProgress();
    }
};
static LocalProgress g_localProgress;
static bool copyTree(const std::string& source, const std::string& target) {
    return sfm::storage::copyTree(source, target, &g_localProgress, g_err);
}
static bool verifyTreeCopy(const std::string& source, const std::string& target) {
    return sfm::storage::verifyTreeCopy(source, target, &g_localProgress, g_err);
}
static bool removeTree(const std::string& path) {
    return sfm::storage::removeTree(path, &g_localProgress, g_err);
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

// Preflight: calcula o total antes do download e falha de forma explícita
// quando a listagem do Drive não está completa. Não modifica arquivos locais.
static bool scanDriveTree(const Entry& e, int depth, std::set<std::string>& seen) {
    if (g_cancel) { g_err = "Operação cancelada durante a análise do Drive."; return false; }
    if (e.gdoc) return true; // formatos nativos Google não são exportados nesta versão
    if (!e.dir) {
        if (UINT64_MAX - g_pTotal < e.size) { g_err = "Tamanho total excede o limite."; return false; }
        g_pTotal += e.size;
        ++g_pFilesTotal;
        return true;
    }
    if (depth > 20) { g_err = "Pasta do Drive excede 20 níveis."; return false; }
    if (e.id.empty() || !seen.insert(e.id).second) {
        g_err = "Pasta repetida ou identificador inválido no Drive."; return false;
    }
    std::vector<gd::Item> items;
    if (!gd::listFolder(e.id, items)) {
        g_err = "Falha ao listar " + e.name + ": " + gd::lastError();
        seen.erase(e.id);
        return false;
    }
    for (const auto& it : items) {
        Entry child;
        child.name = it.name; child.id = it.id; child.dir = it.dir;
        child.gdoc = it.gdoc; child.size = it.size;
        if (!scanDriveTree(child, depth + 1, seen)) { seen.erase(e.id); return false; }
    }
    seen.erase(e.id);
    return true;
}

// baixa um item (arquivo ou pasta) do Drive para dentro de dstDir
static bool dlTree(const Entry& e, const std::string& dstDir, std::string& outPath, int depth) {
    if (g_cancel) return false;
    if (e.gdoc) { g_skipped++; return true; }
    g_pName = e.name;
    std::string dst = uniqueName(join(dstDir, safeName(e.name)));
    if (depth == 0) outPath.clear();
    if (e.dir) {
        if (depth > 20) { g_err = "Pasta do Drive excede 20 níveis."; return false; }
        if (mkdir(dst.c_str(), 0777) != 0) {
            g_err = "Não foi possível criar pasta no SD: " + dst;
            return false;
        }
        if (depth == 0) outPath = dst;  // somente a pasta que este pedido conseguiu criar
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
    if (g_pCountFiles) ++g_pFilesDone;
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
    if (g_pCountFiles) ++g_pFilesDone;
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
    if (move && (s.drive || d.drive)) {
        message("Movimentação protegida", "Mover entre cartão SD e Google Drive está temporariamente bloqueado.\nUse Copiar e confira os arquivos antes de excluir a origem.");
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
    beginTransferProgress();
    if (s.drive) {
        showBusy("Analisando pastas e arquivos do Drive...");
        std::set<std::string> seen;
        bool scanOk = true;
        for (const auto& e : sel) {
            if (!scanDriveTree(e, 0, seen)) { scanOk = false; break; }
        }
        if (!scanOk) {
            message("Google Drive", "Download não iniciado. " + wrapText(g_err, 52));
            return;
        }
    } else {
        for (auto& e : sel) {
            const std::string src = join(s.path, e.name);
            g_pTotal += treeSize(src);
            countLocalFiles(src);
        }
    }
    // Excluir tempo gasto na contagem inicial da velocidade media.
    g_pStartMs = progressNowMs();
    tickProgress(true);

    int fail = 0;
    for (auto& e : sel) {
        if (g_cancel) break;
        bool ok = true;
        if (s.drive) {
            std::string outPath;
            ok = dlTree(e, d.path, outPath, 0);
            if (!ok && !outPath.empty() && !removeTree(outPath))
                g_err += " ATENÇÃO: download incompleto pode permanecer em " + outPath;
            if (ok && move && !e.gdoc && !gd::trashItem(e.id)) { ok = false; g_err = gd::lastError(); }
        } else if (d.drive) {
            std::string from = join(s.path, e.name);
            // A API de upload não oferece aqui uma conferência integral do arquivo remoto.
            // Por segurança, não remover a origem em operações de mover para o Drive.
            if (move) {
                ok = false;
                g_err = "Mover para o Drive está bloqueado: use Copiar e confira o destino antes de apagar a origem.";
            } else {
                ok = ulTree(from, d.driveId());
            }
        } else {
            std::string from = join(s.path, e.name);
            std::string to = uniqueName(join(d.path, e.name));
            if (pathExists(to)) {
                fail++;
                g_err = "Destino já existe; nenhum arquivo foi sobrescrito: " + e.name;
                continue;
            }
            if (e.dir && (to == from || to.compare(0, from.size() + 1, from + "/") == 0)) {
                fail++;
                g_err = "Não é possível copiar uma pasta para dentro dela mesma.";
                continue;
            }
            if (move && rename(from.c_str(), to.c_str()) == 0) {
                // Renomeacao no mesmo volume e instantanea, sem transferencia de bytes.
                g_pDone += treeSize(to);
                g_pFilesDone += 1;
                ok = true;
            } else {
                const bool copied = copyTree(from, to);
                ok = copied;
                if (ok && move) ok = verifyTreeCopy(from, to);
                if (!ok) {
                    // Em caso de falha, manter a origem e tentar limpar a cópia incompleta.
                    std::string cause = g_err;
                    if (copied && !removeTree(to))
                        g_err = cause + " ATENÇÃO: restos da cópia podem permanecer em " + to;
                    else
                        g_err = cause;
                } else if (move) {
                    // Só excluir a origem depois da comparação integral bem-sucedida.
                    ok = removeTree(from);
                }
            }
        }
        if (!ok) fail++;
    }
    reloadBoth();
    if (g_cancel) message("Cancelado", wrapText(
        g_err.find("ATENÇÃO:") != std::string::npos ? g_err :
        std::string("Operação interrompida. A origem foi preservada quando a cópia não foi concluída. ") +
        "Confira o destino antes de reutilizar os arquivos.", 52));
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
    g_err.clear();
    int failed = 0;
    for (auto& e : sel) if (!removeTree(join(p.path, e.name))) failed++;
    reloadBoth();
    if (failed) message("Exclusão incompleta", std::to_string(failed) +
        " item(ns) não puderam ser removidos.\n" + wrapText(g_err, 48));
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

// Fila v0.8.0: instantâneos locais; nenhuma operação ocorre ao adicionar.
struct QueuedTransfer {
    std::string source, destination;
    std::vector<Entry> entries;
    bool move = false;
};
static std::vector<QueuedTransfer> g_transferQueue;
static const size_t QUEUE_LIMIT = 20;

static void enqueueTransfer(bool move) {
    Pane& source = panes[active];
    Pane& destination = panes[1 - active];
    if (source.drive || destination.drive) {
        message("Fila local", "A fila aceita apenas transferências entre pastas do cartão SD nesta versão. Para Google Drive, use a operação direta.");
        return;
    }
    auto chosen = selection(source);
    if (chosen.empty()) { message("Fila", "Selecione um arquivo ou pasta primeiro."); return; }
    if (source.path == destination.path) {
        message("Fila", "Abra pastas diferentes nos dois painéis."); return;
    }
    if (g_transferQueue.size() >= QUEUE_LIMIT) {
        message("Fila cheia", "Execute ou limpe a fila antes de adicionar mais tarefas."); return;
    }
    QueuedTransfer job;
    job.source = source.path;
    job.destination = destination.path;
    job.entries = chosen;
    job.move = move;
    g_transferQueue.push_back(job);
    message("Adicionado à fila", std::to_string(chosen.size()) + " item(ns). Tarefas na fila: " + std::to_string(g_transferQueue.size()) + ". Nenhum arquivo foi alterado.");
}

static void runTransferQueue() {
    if (g_transferQueue.empty()) { message("Fila vazia", "Adicione tarefas antes de executar."); return; }
    // Manter a interface do usuário e suas marcações intactas ao processar a fila.
    Pane saved0 = panes[0], saved1 = panes[1];
    int savedActive = active;
    size_t finished = 0;
    bool interrupted = false;
    while (!g_transferQueue.empty()) {
        QueuedTransfer job = g_transferQueue.front();
        Pane source, destination;
        source.path = job.source;
        destination.path = job.destination;
        source.items = job.entries;
        source.cur = 0;
        for (const auto& e : job.entries) source.marks.insert(e.key);
        panes[0] = source;
        panes[1] = destination;
        active = 0;
        // doCopyMove verifica destino e preserva a origem em falhas de cópia.
        doCopyMove(job.move);
        interrupted = g_cancel || !g_err.empty();
        if (interrupted) break;
        g_transferQueue.erase(g_transferQueue.begin());
        ++finished;
    }
    panes[0] = saved0;
    panes[1] = saved1;
    active = savedActive;
    reloadBoth();
    if (interrupted) {
        message("Fila interrompida", std::to_string(finished) + " tarefa(s) concluída(s). A tarefa interrompida e as restantes continuam na fila. Confira os arquivos antes de tentar novamente.");
    } else {
        message("Fila concluída", std::to_string(finished) + " tarefa(s) processada(s).");
    }
}

static void transferQueueMenu() {
    const std::vector<std::string> options = {
        "Adicionar cópia do painel ativo", "Adicionar movimentação do painel ativo",
        "Executar fila", "Ver tarefas pendentes", "Limpar fila (sem apagar arquivos)"
    };
    int choice = modal("Fila de transferências", std::to_string(g_transferQueue.size()) + " tarefa(s) pendente(s) / limite 20", options);
    if (choice == 0) enqueueTransfer(false);
    else if (choice == 1) enqueueTransfer(true);
    else if (choice == 2) runTransferQueue();
    else if (choice == 3) {
        if (g_transferQueue.empty()) { message("Fila vazia", "Nenhuma tarefa pendente."); return; }
        std::vector<std::string> labels;
        for (const auto& job : g_transferQueue)
            labels.push_back(std::string(job.move ? "Mover: " : "Copiar: ") + std::to_string(job.entries.size()) + " item(ns) -> " + job.destination);
        modal("Tarefas pendentes", "A fila é temporária e não persiste após fechar o aplicativo.", labels);
    } else if (choice == 4 && !g_transferQueue.empty()) {
        if (modal("Limpar fila", "Descartar tarefas pendentes? Nenhum arquivo será apagado.", {"Cancelar", "Limpar fila"}) == 1) {
            g_transferQueue.clear();
            message("Fila limpa", "Todas as tarefas pendentes foram descartadas.");
        }
    }
}

static void wifiMenu() {
    if (!wifi::running()) {
        if (modal("Servidor FTP",
                  "Ativar acesso total ao cartao SD?\n"
                  "Leitura e gravacao, sem senha.\n"
                  "Use somente no Wi-Fi confiavel.\n"
                  "Desligue quando terminar.",
                  {"Cancelar", "Iniciar FTP"}) != 1) return;
        std::string err;
        if (!wifi::start(err)) { message("FTP indisponivel", err); return; }
    }
    std::string last = wifi::lastActivity();
    if (last.size() > 38) last = last.substr(0, 35) + "...";
    int choice = modal("Servidor FTP ativo",
        "Endereco FTP:\n" + wifi::url() +
        "\nRaiz: / (cartao SD completo)" +
        "\nAcesso anonimo, sem senha" +
        "\nClientes conectados: " + std::to_string(wifi::activeClients()) +
        "\nArquivos concluidos: " + std::to_string(wifi::filesCompleted()) +
        "\nTotal transferido: " + fmtSize(wifi::bytesTransferred()) +
        "\nUltima atividade:" +
        "\n" + last,
        {"Manter ativo", "Parar servidor FTP"});
    if (choice == 1) wifi::stop();
}

static void actionsMenu() {
    std::vector<std::string> o = {
        "Copiar para o outro painel", "Mover para o outro painel", "Renomear",
        "Nova pasta", "Apagar", "Seleção múltipla...",
        "Fila de transferências...", "Ordenar e buscar...", "Compactar e extrair (ZIP)...", "Propriedades e espaço...", "Favoritos e histórico...", "Google Drive e conta...", "Transferência Wi-Fi..."};
    int r = modal("Ações", "", o);
    if (r >= 0 && r < (int)o.size()) sfm_diag::write("ACAO", o[r]);
    switch (r) {
        case 0: doCopyMove(false); break;
        case 1: doCopyMove(true); break;
        case 2: doRename(); break;
        case 3: doMkdir(); break;
        case 4: doDelete(); break;
        case 5: selectionMenu(); break;
        case 6: transferQueueMenu(); break;
        case 7: sortMenu(); break;
        case 8: zipMenu(); break;
        case 9: propsMenu(); break;
        case 10: favMenu(); break;
        case 11: cloudMenu(); break;
        case 12: wifiMenu(); break;
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
    sfm_diag::write("APP", "Inicio v" SFM_VERSION);

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    hidInitializeTouchScreen();

    const bool graphicsReady = initialize();

    if (graphicsReady) {
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
            static unsigned lastFtpChanges = 0;
            unsigned ftpChanges = wifi::fileChanges();
            if (ftpChanges != lastFtpChanges) {
                lastFtpChanges = ftpChanges;
                for (int pi = 0; pi < 2; ++pi)
                    if (!panes[pi].drive) panes[pi].reload();
                refreshSpace();
            }
            if (++frame % 300 == 0) refreshSpace();
            drawMain();
            present();
        }
    }

    sfm_diag::write("APP", "Encerramento normal");
    wifi::stop();
    gd::netExit();
    shutdown();
    return graphicsReady ? 0 : 1;
}
