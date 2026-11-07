"""Teste de unidade independente de libnx para o menu contextual 0.10.13."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

CPP = r"""
#include "ui/context_actions.h"
#include <cassert>
using namespace sfm::ui::actions;
int main() {
    assert(contextualCommands({false, 0, 0, false, false}).empty());
    assert(contextualCommands({false, 1, 0, false, false}).empty());
    assert(contextualCommands({true, 0, 0, false, false}).empty());

    auto file = contextualCommands({true, 1, 0, false, false});
    assert(file.size() == 6);
    assert(file[0] == Command::Copy && file[1] == Command::Move);
    assert(file[2] == Command::SelectAll && file[3] == Command::Delete);
    assert(file[4] == Command::Rename && file[5] == Command::CompressZip);

    auto folder = contextualCommands({true, 1, 0, false, false});
    assert(folder.back() == Command::CompressZip);

    auto oneZip = contextualCommands({true, 1, 0, true, false});
    assert(oneZip.size() == 6 && oneZip.back() == Command::ExtractZip);

    auto several = contextualCommands({true, 3, 3, false, false});
    assert(several.size() == 5 && several[2] == Command::ClearAll);
    assert(several[4] == Command::CompressZip);
    assert(several[3] == Command::Delete);

    auto zips = contextualCommands({true, 4, 4, true, false});
    assert(zips.size() == 5 && zips.back() == Command::ExtractZip);

    auto drive = contextualCommands({true, 1, 0, true, true});
    assert(drive.size() == 4 && drive.back() == Command::Rename);

    auto markedWithCursor = contextualCommands({false, 2, 2, false, false});
    assert(markedWithCursor.size() == 5);
    assert(markedWithCursor[2] == Command::ClearAll);
}
"""

class ContextMenuTests(unittest.TestCase):
    def test_context_menu_cpp(self):
        compiler = shutil.which("g++")
        self.assertIsNotNone(compiler, "g++ é necessário para validar a lógica do menu")
        with tempfile.TemporaryDirectory(prefix="sfm_context_") as directory:
            source = Path(directory) / "menu.cpp"
            executable = Path(directory) / "menu"
            source.write_text(CPP, encoding="utf-8")
            subprocess.run(
                [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                 "-I", str(ROOT / "include"), str(source), "-o", str(executable)],
                check=True, capture_output=True, text=True,
            )
            subprocess.run([str(executable)], check=True, timeout=15)

    def test_no_old_menu_and_pix_only_in_about(self):
        text = (ROOT / "src/app/main.cpp").read_text(encoding="utf-8")
        about = (ROOT / "src/ui/about_view.cpp").read_text(encoding="utf-8")
        self.assertIn("contextualCommands(state)", text)
        self.assertIn("static void globalMenu()", text)
        self.assertIn("static void aboutScreen()", text)
        self.assertNotIn('"Compactar e extrair (ZIP)..."', text)
        self.assertIn("blitRgb24(112, 121", about)
        self.assertIn("blitRgb24(112, 438", about)
        self.assertNotIn('"Ferramentas"', text)
        self.assertNotIn('"Configurações do atualizador"', text)
        self.assertIn('{"Atualizações", "Versão e diagnósticos"}', text)

    def test_pix_matrix(self):
        header = (ROOT / "include/ui/pix_qr.h").read_text(encoding="utf-8")
        import re
        rows = re.findall(r'^"([A-F0-9]{14})"', header, flags=re.MULTILINE)
        self.assertEqual(len(rows), 53)
        self.assertTrue(all(int(row[-1], 16) % 2 == 0 for row in rows))
        self.assertTrue((ROOT / "assets/pix_doacoes.svg").exists())

if __name__ == "__main__":
    unittest.main()
