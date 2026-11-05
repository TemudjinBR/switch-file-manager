#include "ui/renderer.h"
#include "sfm_diag.h"
#include <switch.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"
namespace sfm::ui {
static Framebuffer g_fb;
static bool framebufferReady = false, plReady = false;
static uint32_t* canvas = nullptr;
Fnt fS, fM, fL;
static stbtt_fontinfo g_info;
static bool g_fontOk = false;
static int clipX0 = 0, clipY0 = 0, clipX1 = W, clipY1 = H;
void setClip(int x, int y, int w, int h) {
    clipX0 = std::max(0, x); clipY0 = std::max(0, y);
    clipX1 = std::min(W, x + w); clipY1 = std::min(H, y + h);
}
void resetClip() { clipX0 = 0; clipY0 = 0; clipX1 = W; clipY1 = H; }

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

void fillRect(int x, int y, int w, int h, Col c) {
    for (int j = y; j < y + h; j++) hspan(j, x, x + w, c);
}

void fillRound(int x, int y, int w, int h, int r, Col c) {
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

int textWidth(const Fnt& f, const std::string& str) {
    const char* s = str.c_str();
    float w = 0;
    while (*s) w += getGlyph(f, nextCp(s)).adv;
    return (int)(w + 0.5f);
}

std::string fitText(const Fnt& f, std::string s, int maxW) {
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
void drawText(const Fnt& f, std::string s, int x, int y, Col c, int maxW, int align) {
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

void present() {
    u32 stride;
    uint32_t* fb = (uint32_t*)framebufferBegin(&g_fb, &stride);
    for (int y = 0; y < H; y++) memcpy((uint8_t*)fb + (size_t)y * stride, canvas + (size_t)y * W, W * 4);
    framebufferEnd(&g_fb);
}

bool initialize() {
    Result pl = plInitialize(PlServiceType_User);
    if (R_FAILED(pl)) { sfm_diag::write("ERRO", "Fonte do sistema indisponível"); return false; }
    plReady = true;
    canvas = (uint32_t*)malloc((size_t)W * H * 4);
    if (!canvas) { sfm_diag::write("ERRO", "Memória insuficiente para a interface"); return false; }
    Result rc = framebufferCreate(&g_fb, nwindowGetDefault(), W, H, PIXEL_FORMAT_RGBA_8888, 2);
    if (R_FAILED(rc)) { sfm_diag::write("ERRO", "Falha ao criar framebuffer"); return false; }
    framebufferReady = true;
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

    if (!g_fontOk) sfm_diag::write("ERRO", "Falha ao carregar fonte da interface");
    return g_fontOk;
}
void shutdown() {
    for (auto& entry : g_glyphs) stbtt_FreeBitmap(entry.second.bmp, nullptr);
    g_glyphs.clear();
    if (framebufferReady) framebufferClose(&g_fb);
    framebufferReady = false;
    free(canvas); canvas = nullptr;
    if (plReady) plExit();
    plReady = false; g_fontOk = false;
}
}
