#include "ui/menu_view.h"
#include "ui/navigation.h"
#include "ui/renderer.h"
#include <algorithm>
namespace sfm::ui {
MenuLayout menuLayout(int count, int lines) {
  MenuLayout m;
  m.rowH = count > 8 ? 46 : 54;
  int textH = lines == 0 ? 0 : lines * m.lineH + 14;
  int maxVis = std::max(3, (H - 40 - 84 - textH - 22) / m.rowH);
  m.vis = std::min(count, maxVis);
  m.bh = 84 + textH + m.vis * m.rowH + 22;
  m.bx = (W - m.bw) / 2;
  m.by = (H - m.bh) / 2;
  m.oy0 = m.by + 84 + textH;
  return m;
}
void drawFooter() {
  for (unsigned i = 0; i < navigation::footer.size(); ++i) {
    int bx = 22 + int(i) * 178;
    fillRound(bx, 646, 168, 58, 12, C_PANEL);
    fillRound(bx + 8, 655, 44, 39, 9, C_PAN2);
    const bool shoulder = std::string(navigation::footer[i].key) == "L/R";
    drawText(shoulder ? fS : fM, navigation::footer[i].key, bx + 30,
             shoulder ? 663 : 660, C_ACC, 40, 2);
    drawText(fM, navigation::footer[i].label, bx + 58, 660, C_TXT, 102);
  }
}
void drawMenu(const std::string &title, const std::vector<std::string> &lines,
              const std::vector<std::string> &options, int selected, int top,
              const MenuLayout &m) {
  fillRect(0, 0, W, H, Col{0, 0, 0, 175});
  fillRound(m.bx, m.by, m.bw, m.bh, 18, C_PANEL);
  drawText(fL, title, m.bx + 28, m.by + 22, C_TXT, m.bw - 56);
  for (unsigned i = 0; i < lines.size(); ++i)
    drawText(fM, lines[i], m.bx + 28, m.by + 80 + int(i) * m.lineH, C_DIM,
             m.bw - 56);
  for (int k = 0; k < m.vis; ++k) {
    int i = top + k, oy = m.oy0 + k * m.rowH;
    bool danger = options[i].rfind("Apagar", 0) == 0;
    fillRound(m.bx + 20, oy, m.bw - 40, m.rowH - 6, 12,
              i == selected ? C_ACCD : C_PAN2);
    drawText(fM, options[i], m.bx + m.bw / 2, oy + (m.rowH - 54) / 2 + 9,
             danger ? C_RED : C_TXT, m.bw - 90, 2);
  }
  if (int(options.size()) > m.vis) {
    int trackH = m.vis * m.rowH - 6, n = int(options.size());
    int thumbH = std::max(24, trackH * m.vis / n);
    int thumbY = m.oy0 + (trackH - thumbH) * top / std::max(1, n - m.vis);
    fillRound(m.bx + m.bw - 14, thumbY, 4, thumbH, 2, C_DIM);
  }
}
} // namespace sfm::ui
