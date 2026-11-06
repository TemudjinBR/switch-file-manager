#pragma once
#include <cstdint>
#include <string>
namespace sfm::ui {
struct Col { uint8_t r, g, b, a; };
inline constexpr int W = 1280, H = 720;
inline constexpr int ROWS = 10, ROW_H = 46, LIST_TOP = 116, PANE_Y = 72, PANE_H = 560, PANE_W = 610;

inline constexpr Col C_BG    = {11, 15, 24, 255};
inline constexpr Col C_PANEL = {22, 29, 43, 255};
inline constexpr Col C_PAN2  = {35, 47, 66, 255};
inline constexpr Col C_SEL   = {43, 57, 77, 255};
inline constexpr Col C_ACC   = {62, 174, 241, 255};
inline constexpr Col C_ACCD  = {35, 105, 156, 255};
inline constexpr Col C_TXT   = {232, 237, 247, 255};
inline constexpr Col C_DIM   = {125, 137, 163, 255};
inline constexpr Col C_RED   = {244, 106, 106, 255};
inline constexpr Col C_GRN   = {84, 214, 140, 255};
inline constexpr Col C_YEL   = {250, 204, 21, 255};

struct Fnt { int px; float scale; int asc; };
extern Fnt fS, fM, fL;
bool initialize();
void shutdown();
void setClip(int x, int y, int w, int h);
void resetClip();
void fillRect(int x, int y, int w, int h, Col c);
void fillRound(int x, int y, int w, int h, int r, Col c);
int textWidth(const Fnt& f, const std::string& str);
std::string fitText(const Fnt& f, std::string s, int maxW);
void drawText(const Fnt& f, std::string s, int x, int y, Col c, int maxW = 0, int align = 0);
void present();
}
