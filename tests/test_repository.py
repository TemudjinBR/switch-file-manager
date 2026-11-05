from pathlib import Path
import hashlib
import re
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]


class RepositoryTests(unittest.TestCase):
    def test_required_documentation(self):
        names = ['README.md', 'CHANGELOG.md', 'docs/REFERENCIA.md', 'docs/ARQUITETURA.md',
                 'docs/TESTES.md', 'docs/ROADMAP.md', 'docs/PROBLEMAS_CONHECIDOS.md',
                 'docs/AUDITORIA.md', 'docs/RELATORIO_VALIDACAO.md', f'release-notes/v{(ROOT / "VERSION").read_text().strip()}.md']
        for name in names:
            with self.subTest(path=name):
                self.assertGreater((ROOT / name).stat().st_size, 100)

    def test_modules_are_populated(self):
        for name in ('app', 'core', 'ui', 'storage', 'network', 'cloud', 'archives', 'diagnostics', 'security'):
            self.assertTrue(list((ROOT / 'src' / name).glob('*.cpp')), name)
        self.assertFalse(list(ROOT.glob('*.cpp')))
        self.assertTrue((ROOT / 'assets/icon.jpg').is_file())
        self.assertTrue((ROOT / 'third_party/miniz/miniz.c').is_file())
        self.assertTrue((ROOT / 'third_party/stb/stb_truetype.h').is_file())

    def test_local_documentation_links(self):
        documents = [ROOT / 'README.md', ROOT / 'CHANGELOG.md', *[p for p in (ROOT / 'docs').glob('*.md')]]
        for document in documents:
            for target in re.findall(r'\[[^\]]*\]\(([^)]+)\)', document.read_text()):
                if re.match(r'^(https?:|#|mailto:)', target): continue
                path = document.parent / target.split('#')[0]
                self.assertTrue(path.exists(), f'{document.name}: {target}')

    def test_preparation_and_legacy_entries_do_not_mutate_sources(self):
        files = [ROOT / 'Makefile', ROOT / 'VERSION']
        for directory in ('src', 'include', 'assets', 'third_party'):
            files.extend(p for p in (ROOT / directory).rglob('*') if p.is_file())
        def hashes():
            return {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
        original = hashes()
        scripts = [ROOT / 'scripts/prepare.py', *sorted((ROOT / 'scripts/legacy').glob('apply_*.py'))]
        for _ in range(2):
            for script in scripts:
                subprocess.run([sys.executable, str(script)], cwd=ROOT.parent, check=True, capture_output=True)
        self.assertEqual(original, hashes())

    def test_scripts_have_valid_syntax(self):
        for script in (ROOT / 'scripts').glob('*.sh'):
            subprocess.run(['bash', '-n', str(script)], check=True, capture_output=True)
        for script in [*(ROOT / 'scripts').rglob('*.py'), *(ROOT / 'tests').glob('*.py')]:
            compile(script.read_text(), str(script), 'exec')
