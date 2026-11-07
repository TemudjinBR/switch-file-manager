"""Regressões executáveis da política usada pela UI e cancelamento local real."""
from pathlib import Path
import hashlib
import re
import shutil
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]


class NavigationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory(prefix='sfm_navigation_')
        cls.executable = Path(cls.directory.name) / 'navigation'
        subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                        '-I', str(ROOT / 'include'),
                        str(ROOT / 'tests/host/navigation.cpp'),
                        str(ROOT / 'src/storage/operations.cpp'),
                        str(ROOT / 'src/storage/filesystem.cpp'),
                        str(ROOT / 'src/core/paths.cpp'),
                        str(ROOT / 'src/security/path_policy.cpp'),
                        '-o', str(cls.executable)], check=True, capture_output=True, text=True)

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def run_case(self, name):
        subprocess.run([str(self.executable), name], check=True, timeout=15)

    def test_global_in_both_panes_sd_drive_zero_one_many_preserves_context(self):
        self.run_case('global')

    def test_context_only_and_advanced_combinations(self):
        self.run_case('context')

    def test_footer_touch_boundaries_and_same_commands(self):
        self.run_case('touch')

    def test_preserved_buttons_and_injected_libnx_mapping(self):
        self.run_case('preserved-controls')

    def test_confirm_and_refuse_safe_exit_with_pending_menu(self):
        self.run_case('safe-exit')

    def test_exit_during_actual_copy_confirmed_or_refused(self):
        for confirmed in (True, False):
            with self.subTest(confirmed=confirmed), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                data = b'fixture-sem-dados-reais' * 40000
                (root / 'source').write_bytes(data)
                (root / 'personal').write_text('preservar')
                name = 'copy-exit-confirmed' if confirmed else 'copy-exit-refused'
                subprocess.run([str(self.executable), name, directory], check=True, timeout=15)
                self.assertEqual((root / 'source').read_bytes(), data)
                self.assertEqual((root / 'personal').read_text(), 'preservar')
                if not confirmed:
                    self.assertEqual((root / 'target').read_bytes(), data)

    def test_native_integration_uses_shared_policy_and_ordered_shutdown(self):
        code = (ROOT / 'src/app/main.cpp').read_text()
        self.assertIn('nav::buttons(d, h, keys)', code)
        self.assertIn('nav::touch(tp.x, tp.y)', code)
        self.assertIn('g_session.exit(choice == 1)', code)
        self.assertLess(code.index('g_updates.stop();'), code.index('gd::setNetworkPump({});'))
        self.assertLess(code.index('wifi::stop();', code.index('g_updates.stop();')),
                        code.index('gd::netExit();', code.index('g_updates.stop();')))
        self.assertNotIn('B_CLOUD', code)
        self.assertIn('else if (choice == 4) toggleDrive();', code)
        system = code[code.index('static void systemMenu()'):code.index('static void globalMenu() {')]
        self.assertNotIn('Sobre', system)
        self.assertNotIn('Configurações do atualizador', system)


class AboutTests(unittest.TestCase):
    def test_native_rgb_matches_approved_decoding_without_resizing(self):
        header = (ROOT / 'include/ui/official_icon.h').read_text()
        pixels = bytes(int(value) for value in re.findall(r'\d+', header.split('kRgb[] = {')[1].split('};')[0]))
        self.assertEqual(len(pixels), 256 * 256 * 3)
        digest = re.search(r'SHA-256 RGB: ([a-f0-9]{64})', header)[1]
        self.assertEqual(hashlib.sha256(pixels).hexdigest(), digest)
        # Impressão independente fixada após comparar com JPEG oficial, sem quantização.
        self.assertEqual(digest, '3e5c0287dc0e75eef26e2f19f94b4fd8a89030eea277ad6556afe9cbb46fcf62')
        self.assertNotIn('kPalette', header)

    def test_qr_exact_original_svg_matrix_and_asset_hashes(self):
        svg = ROOT / 'assets/pix_doacoes.svg'
        self.assertEqual(hashlib.sha256(svg.read_bytes()).hexdigest(),
                         'b706a3a0c94d27d231648a5539a60abaf710d46638a5dec201fd28c58b08fbc5')
        header = (ROOT / 'include/ui/pix_qr.h').read_bytes()
        self.assertEqual(hashlib.sha256(header).hexdigest(),
                         'f268795998e29a126464a569f44cec8ae25d3c5c2a443856be9bbb37350cdb89')
        rows = re.findall(r'^"([A-F0-9]{14})"', header.decode(), re.M)
        pixels = set()
        for path in ET.parse(svg).getroot().iter('{http://www.w3.org/2000/svg}path'):
            for x, y, length, back in re.findall(r'M(\d+) (\d+)h(\d+)v1h-(\d+)z', path.attrib['d']):
                self.assertEqual(length, back)
                pixels.update((int(x)+offset, int(y)) for offset in range(int(length)))
        expected = {(col+4, row+4) for row, line in enumerate(rows) for col in range(53)
                    if (int(line[col//4], 16) >> (3-col%4)) & 1}
        self.assertEqual(pixels, expected)

    def test_actual_about_renderer_icon_pixels_qr_and_blit_clipping(self):
        font = Path('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf')
        self.assertTrue(font.exists(), 'Fonte de QA do host necessária')
        with tempfile.TemporaryDirectory() as directory:
            executable, preview = Path(directory)/'preview', Path(directory)/'about.ppm'
            subprocess.run(['g++', '-std=c++17', '-O2', '-DHOST_TEST', '-I', str(ROOT/'include'),
                            '-I', str(ROOT/'third_party/stb'), str(ROOT/'tests/host/about_preview.cpp'),
                            str(ROOT/'src/ui/renderer.cpp'), str(ROOT/'src/ui/about_view.cpp'), str(ROOT/'src/ui/menu_view.cpp'),
                            '-o', str(executable)], check=True, capture_output=True, text=True)
            subprocess.run([str(executable), str(font), str(preview)], check=True, timeout=15)
            data = preview.read_bytes().split(b'\n', 3)[3]
            header = (ROOT/'include/ui/official_icon.h').read_text()
            rgb = bytes(int(v) for v in re.findall(r'\d+', header.split('kRgb[] = {')[1].split('};')[0]))
            crop = b''.join(data[((121+y)*1280+112)*3:((121+y)*1280+368)*3] for y in range(256))
            self.assertEqual(crop, rgb)
            rows = re.findall(r'^"([A-F0-9]{14})"', (ROOT/'include/ui/pix_qr.h').read_text(), re.M)
            for y in range(61):
                for x in range(61):
                    dark = (4 <= x < 57 and 4 <= y < 57 and
                            (int(rows[y-4][(x-4)//4], 16) >> (3-(x-4)%4)) & 1)
                    offset = ((438+y*4+2)*1280+112+x*4+2)*3
                    self.assertEqual(data[offset:offset+3], b'\0'*3 if dark else b'\xff'*3)
