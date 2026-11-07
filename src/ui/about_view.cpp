#include "ui/about_view.h"
#include "ui/official_icon.h"
#include "ui/pix_qr.h"
#include "ui/renderer.h"
#include <array>
namespace sfm::ui {
namespace {
// Matriz original, apenas expandida para RGB uma única vez. Quiet zone
// preservada.
const auto &donationPixels() {
  static const auto pixels = [] {
    std::array<uint8_t, 61 * 61 * 3> rgb{};
    rgb.fill(255);
    for (int row = 0; row < pix::kSize; ++row)
      for (int col = 0; col < pix::kSize; ++col) {
        char c = pix::kRows[row][col / 4];
        unsigned nibble = c >= 'A' ? unsigned(c - 'A' + 10) : unsigned(c - '0');
        if ((nibble >> (3 - col % 4)) & 1) {
          int at = ((row + 4) * 61 + col + 4) * 3;
          rgb[at] = rgb[at + 1] = rgb[at + 2] = 0;
        }
      }
    return rgb;
  }();
  return pixels;
}
} // namespace
void drawAbout(const AboutInfo &info) {
  fillRect(0, 0, W, H, Col{0, 0, 0, 190});
  fillRound(60, 22, 1160, 676, 20, C_PANEL);
  fillRect(60, 42, 7, 636, C_ACC);
  drawText(fL, "Sobre o Switch File Manager", 112, 56, C_TXT);
  // Arte oficial em RGB888, 256x256 nativos, sem paleta nem ampliação.
  blitRgb24(112, 121, brand::kIconSize, brand::kIconSize, brand::kRgb);
  drawText(fM, "Gerenciador de arquivos para Nintendo Switch", 410, 126, C_TXT,
           760);
  drawText(fS, "Dois painéis | microSD | FTP | ZIP", 410, 173, C_DIM);
  drawText(fS, "Google Drive experimental", 410, 207, C_DIM);
  drawText(fS, "Desenvolvedor: TemudjinBR", 410, 248, C_TXT);
  drawText(fS, "Versão: " + info.version + " (experimental)", 410, 282, C_TXT);
  drawText(fS, "Build: " + info.build, 410, 316, C_DIM, 760);
  drawText(fS, "Canal de atualizações: " + info.updateChannel, 410, 350, C_DIM);
  drawText(fS, "github.com/TemudjinBR/switch-file-manager", 410, 384, C_ACC,
           760);
  fillRect(112, 420, 1056, 2, C_PAN2);
  blitRgb24(112, 438, 61, 61, donationPixels().data(), 4);
  drawText(fM, "Apoie o desenvolvimento", 410, 441, C_TXT);
  drawText(fS, "Contribuição voluntária, no valor que você escolher.", 410, 486,
           C_TXT, 760);
  drawText(fS, "O aplicativo continua gratuito, com todos os recursos.", 410,
           520, C_GRN, 760);
  drawText(fS, "Beneficiário: Flavio Hessnauer Filho", 410, 562, C_DIM);
  drawText(fS, "QR Pix original. Confira o destinatário no seu banco.", 410,
           596, C_DIM, 760);
  fillRound(918, 637, 240, 46, 10, C_ACCD);
  drawText(fM, "B  Voltar", 1038, 644, C_TXT, 230, 2);
}
} // namespace sfm::ui
