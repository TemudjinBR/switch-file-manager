#pragma once
#include <string>
#include <vector>
namespace sfm::ui {
struct MenuLayout {
  int bw = 640, lineH = 30, rowH = 54, vis = 0, bh = 0, bx = 0, by = 0, oy0 = 0;
};
MenuLayout menuLayout(int count, int lines);
void drawMenu(const std::string &title, const std::vector<std::string> &lines,
              const std::vector<std::string> &options, int selected, int top,
              const MenuLayout &layout);
void drawFooter();
} // namespace sfm::ui
