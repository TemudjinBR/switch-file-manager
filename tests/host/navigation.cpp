#include "ui/navigation.h"
#include "storage/operations.h"
#include "ui/context_actions.h"
#include <cassert>
#include <fstream>
#include <string>
#include <vector>
using namespace sfm::ui;
namespace nav = sfm::ui::navigation;
struct Progress : sfm::storage::Progress {
  nav::Session session;
  bool confirm;
  explicit Progress(bool value) : confirm(value) {}
  bool cancelled() const override { return session.cancel; }
  void update(const std::string &, uint64_t, bool) override {
    session.exit(confirm);
  }
};
int main(int argc, char **argv) {
  assert(argc >= 2);
  nav::Buttons k;
  std::string test = argv[1];
  if (test == "global") {
    for (int pane = 0; pane < 2; ++pane)
      for (bool cloud : {false, true})
        for (size_t count : {0u, 1u, 4u})
          for (bool focused : {false, true}) {
            // A mesma política não consulta nem modifica o estado dos painéis.
            actions::Context context{focused, count, count > 1 ? count : 0,
                                     false, cloud};
            auto before = actions::contextualCommands(context);
            assert(nav::buttons(k.plus, 0) == nav::Action::Menu);
            nav::Session session;
            session.menu();
            assert(session.menuPending && !session.quit && !session.cancel);
            assert(context.focused == focused && context.selected == count);
            assert(actions::contextualCommands(context) == before);
            (void)pane;
          }
    assert(nav::sections.size() == 5);
    assert(std::string(nav::sections[4]) == "Sobre");
  } else if (test == "context") {
    for (bool cloud : {false, true}) {
      assert(actions::contextualCommands({false, 0, 0, false, cloud}).empty());
      assert(nav::buttons(k.x, 0) == nav::Action::Context);
      auto one = actions::contextualCommands({true, 1, 0, true, cloud});
      auto multi = actions::contextualCommands({true, 3, 3, true, cloud});
      for (auto command : multi)
        assert(command != actions::Command::Rename);
      for (auto command : one)
        if (cloud) {
          assert(command != actions::Command::Move);
          assert(command != actions::Command::ExtractZip);
        }
      assert(nav::buttons(k.x, k.zl) == nav::Action::Properties);
      assert(nav::buttons(k.y, k.zl) == nav::Action::Selection);
      assert(nav::buttons(k.y, 0) == nav::Action::Mark);
    }
  } else if (test == "touch") {
    for (unsigned i = 0; i < nav::footer.size(); ++i) {
      int x = 22 + int(i) * 178;
      auto action = nav::footer[i].action;
      assert(nav::touch(x, 646) == action);
      assert(nav::touch(x + 167, 703) == action);
      assert(nav::touch(x + 168, 703) == nav::Action::None);
      assert(nav::touch(x, 704) == nav::Action::None);
    }
    assert(nav::touch(912, 675) == nav::Action::Menu);
    assert(nav::touch(1090, 675) == nav::Action::Exit);
    assert(nav::touch(557, 675) == nav::Action::Context);
  } else if (test == "preserved-controls") {
    assert(nav::buttons(k.a, 0) == nav::Action::Open);
    assert(nav::buttons(k.b, 0) == nav::Action::Up);
    assert(nav::buttons(k.l, 0) == nav::Action::Pane);
    assert(nav::buttons(k.r, 0) == nav::Action::Pane);
    assert(nav::buttons(k.minus, 0) == nav::Action::Exit);
    assert(nav::buttons(k.plus | k.minus | k.x, k.zl) == nav::Action::Menu);
    // Mapeamento real injetável: não confundir posições de bits no libnx.
    nav::Buttons real{1ull << 23, 1ull << 22, 1ull << 21,
                      1ull << 20, 1ull << 19, 1ull << 18,
                      1ull << 17, 1ull << 16, 1ull << 15};
    assert(nav::buttons(real.plus, 0, real) == nav::Action::Menu);
    assert(nav::buttons(real.minus, 0, real) == nav::Action::Exit);
    assert(nav::buttons(real.x, real.zl, real) == nav::Action::Properties);
  } else if (test == "safe-exit") {
    // Confirmação recusada mantém tarefas, cancelamento e pedido de menu.
    nav::Session session;
    session.menu();
    session.exit(false);
    assert(!session.quit && !session.cancel && session.menuPending);
    session.exit(true);
    assert(session.quit && session.cancel && !session.menuPending);
    session.menu();
    assert(!session.menuPending);
  } else if (test == "copy-exit-confirmed" || test == "copy-exit-refused") {
    assert(argc == 3);
    std::string root = argv[2], error;
    bool confirm = test == "copy-exit-confirmed";
    Progress progress(confirm);
    bool ok = sfm::storage::copyTree(root + "/source", root + "/target",
                                     &progress, error);
    assert(ok != confirm);
    assert(progress.session.quit == confirm &&
           progress.session.cancel == confirm);
    // Origem e arquivo pessoal não são alvos da saída/cancelamento.
    std::ifstream source(root + "/source", std::ios::binary),
        personal(root + "/personal");
    assert(source.good() && personal.good());
    std::string text;
    personal >> text;
    assert(text == "preservar");
  } else
    return 2;
}
