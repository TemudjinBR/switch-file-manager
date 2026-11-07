// Usa o desenho real do Sobre e renderer, substituindo apenas
// fonte/framebuffer.
#include "ui/about_view.h"
#include "ui/menu_view.h"
#include "ui/navigation.h"
#include "ui/renderer.h"
#include <cassert>
#include <fstream>
int main(int argc, char **argv) {
  using namespace sfm::ui;
  assert((argc >= 3 && argc <= 5) && initializePreview(argv[1]));
  const uint8_t rgb[] = {255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255};
  fillRect(0, 0, W, H, C_BG);
  blitRgb24(-1, -1, 2, 2, rgb);
  assert(previewPixels()[0] == 0xffffffffu);
  fillRect(0, 0, W, H, C_BG);
  setClip(3, 3, 1, 1);
  blitRgb24(2, 2, 2, 2, rgb, 2);
  assert(previewPixels()[3 * W + 3] == 0xff0000ffu);
  assert(previewPixels()[3 * W + 4] != 0xff0000ffu);
  resetClip();
  blitRgb24(0, 0, 0, 0, nullptr, 0);
  fillRect(0, 0, W, H, C_BG);
  drawAbout({"0.10.14", argc >= 4 ? argv[3] : "v0.10.14-0123456789ab",
             "EXPERIMENTAL"});
  if (argc == 5) {
    fillRect(0, 0, W, H, C_BG);
    drawFooter();
    std::vector<std::string> options(navigation::sections.begin(),
                                     navigation::sections.end());
    if (std::string(argv[4]) == "system")
      options = {"Atualizações", "Versão e diagnósticos"};
    drawMenu(std::string(argv[4]) == "system" ? "Sistema" : "Menu — v0.10.14",
             {}, options, 0, 0, menuLayout(int(options.size()), 0));
  }
  const uint32_t *pixels = previewPixels();
  assert(pixels[121 * W + 112] != 0); // desenho real e área de imagem
  std::ofstream out(argv[2], std::ios::binary);
  out << "P6\n" << W << " " << H << "\n255\n";
  for (int i = 0; i < W * H; ++i) {
    char rgb24[] = {char(pixels[i]), char(pixels[i] >> 8),
                    char(pixels[i] >> 16)};
    out.write(rgb24, 3);
  }
  assert(out.good());
  shutdown();
}
