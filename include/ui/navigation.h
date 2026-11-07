#pragma once
// Política única de atalhos e rodapé, usada pela UI e pelos testes de host.
#include <array>
#include <cstdint>
namespace sfm::ui::navigation {
enum class Action {
  None,
  Open,
  Up,
  Mark,
  Context,
  Pane,
  Menu,
  Exit,
  Selection,
  Properties
};
struct Buttons {
  uint64_t a = 1, b = 2, x = 4, y = 8, l = 16, r = 32, zl = 64, plus = 128,
           minus = 256;
};
inline Action buttons(uint64_t down, uint64_t held, const Buttons &key = {}) {
  if (down & key.plus)
    return Action::Menu;
  if (down & key.minus)
    return Action::Exit;
  if (down & key.a)
    return Action::Open;
  if (down & key.b)
    return Action::Up;
  if (down & key.y)
    return held & key.zl ? Action::Selection : Action::Mark;
  if (down & key.x)
    return held & key.zl ? Action::Properties : Action::Context;
  if (down & (key.l | key.r))
    return Action::Pane;
  return Action::None;
}
struct Footer {
  const char *key;
  const char *label;
  Action action;
};
inline constexpr std::array<Footer, 7> footer = {
    {{"A", "Abrir", Action::Open},
     {"B", "Voltar", Action::Up},
     {"Y", "Marcar", Action::Mark},
     {"X", "Ações", Action::Context},
     {"L/R", "Painel", Action::Pane},
     {"+", "Menu", Action::Menu},
     {"−", "Sair", Action::Exit}}};
inline Action touch(int x, int y) {
  for (unsigned i = 0; i < footer.size(); ++i) {
    int left = 22 + int(i) * 178;
    if (x >= left && x < left + 168 && y >= 646 && y < 704)
      return footer[i].action;
  }
  return Action::None;
}
inline constexpr std::array<const char *, 5> sections = {
    "Arquivos e pastas", "Transferências", "Organização", "Sistema", "Sobre"};
struct Session {
  bool quit = false, cancel = false, menuPending = false;
  void menu() {
    if (!quit)
      menuPending = true;
  }
  void exit(bool confirmed) {
    if (confirmed) {
      cancel = true;
      quit = true;
      menuPending = false;
    }
  }
};
} // namespace sfm::ui::navigation
