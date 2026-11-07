from pathlib import Path
import hashlib
from html.parser import HTMLParser
import os
import re
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from check_docs import check, anchors, references
from version import read_version
from validate_nro import validate

OFFICIAL_ICON_SHA256 = '858ebcb92444859e77ba2485a9532bedda25254ddc00d3fca72517eb5ff38cca'


class DocumentationTests(unittest.TestCase):
    def test_active_links_anchors_and_images(self):
        self.assertEqual(check(ROOT), [])

    def test_checker_accepts_unicode_anchors_and_html(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'icon.jpg').write_bytes(b'imagem-de-teste')
            (root / 'README.md').write_text('# Início\n\n[Ir](guia.md#ação)\n\n<img src="icon.jpg" alt="Teste">\n')
            (root / 'guia.md').write_text('# Ação\n\n[Voltar](README.md#início)\n')
            self.assertEqual(check(root, [root / 'README.md', root / 'guia.md']), [])

    def test_checker_rejects_broken_references(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            readme = root / 'README.md'
            readme.write_text('# Teste\n\n[Erro](#nao-existe)\n\n![Ausente](nada.png)\n\n<img src="nada.jpg">\n')
            errors = check(root, [readme])
            self.assertTrue(any('âncora inexistente' in e for e in errors))
            self.assertTrue(any('arquivo inexistente' in e for e in errors))
            self.assertTrue(any('sem texto alternativo' in e for e in errors))

    def test_official_icon_is_preserved(self):
        icon = (ROOT / 'assets/icon.jpg').read_bytes()
        self.assertEqual(hashlib.sha256(icon).hexdigest(), OFFICIAL_ICON_SHA256)
        self.assertTrue(icon.startswith(b'\xff\xd8'))

    def test_readme_header_uses_the_official_icon(self):
        readme = (ROOT / 'README.md').read_text()
        header = readme.split('## ', 1)[0]
        self.assertRegex(header, r'<img\s+src="assets/icon.jpg"\s+width="192"\s+height="192"\s+alt="[^"]+"')
        self.assertIn(read_version() + ' experimental', header)
        self.assertIn('https://github.com/TemudjinBR/switch-file-manager/pull/1', header)

    def test_menu_inventory_covers_implemented_options(self):
        code = (ROOT / 'src/app/main.cpp').read_text()
        inventory = (ROOT / 'docs/FUNCIONALIDADES.md').read_text()
        context = (ROOT / 'docs/MENU_CONTEXTUAL.md').read_text()

        # Menu global independente do foco; modelo compartilhado com a UI.
        self.assertIn('contextualCommands(state)', code)
        self.assertIn('static void actionsMenu()', code)
        navigation = (ROOT / 'include/ui/navigation.h').read_text()
        initializer = re.search(r'sections\s*=\s*\{(.*?)\};', navigation, re.S)
        self.assertIsNotNone(initializer)
        categories = re.findall(r'"([^"\\]+)"', initializer[1])
        self.assertEqual(categories, ['Arquivos e pastas', 'Transferências', 'Organização', 'Sistema', 'Sobre'])
        self.assertNotIn('Ferramentas', categories)
        for category in categories:
            self.assertIn(category, context)

        # Opções existentes de seleção avançada, ordenação e fila não somem.
        for function, variable in [('selectionMenu', 'opts'),
                                   ('sortMenu', 'o'), ('transferQueueMenu', 'options')]:
            block = code[code.index('static void ' + function + '('):]
            initializer = re.search(
                r'(?:const )?std::vector<std::string>\s+' + variable + r'\s*=\s*\{(.*?)\};',
                block, re.S)
            self.assertIsNotNone(initializer, function)
            for option in re.findall(r'"([^"\\]+)"', initializer[1]):
                with self.subTest(function=function, option=option):
                    self.assertIn(option, inventory)

        # ZIP não é mais uma categoria do menu geral.
        self.assertNotIn('static void zipMenu()', code)
        for option in ('Compactar em ZIP', 'Extrair ZIP'):
            self.assertIn(option, code)
            self.assertIn(option, context)

    def test_physical_drive_failure_stays_documented(self):
        message = 'Falha ao salvar login no cartão SD.'
        self.assertIn(message, (ROOT / 'src/cloud/gdrive.cpp').read_text())
        for name in ('README.md', 'docs/GOOGLE_DRIVE.md', 'docs/PROBLEMAS_CONHECIDOS.md', 'docs/ROADMAP.md'):
            with self.subTest(document=name):
                self.assertIn(message, (ROOT / name).read_text())

    @unittest.skipUnless(os.environ.get('SFM_TEST_NRO'), 'Execute com --nro para conferir o ícone incorporado')
    def test_compiled_nro_contains_unchanged_official_icon(self):
        path = Path(os.environ['SFM_TEST_NRO'])
        validate(path, read_version())
        data = path.read_bytes()
        executable_size = struct.unpack_from('<I', data, 0x18)[0]
        icon_offset, icon_size = struct.unpack_from('<QQ', data, executable_size + 8)
        icon = data[executable_size + icon_offset:executable_size + icon_offset + icon_size]
        self.assertEqual(icon, (ROOT / 'assets/icon.jpg').read_bytes())
