from pathlib import Path
import os
import subprocess
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]


class ModuleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory(prefix='sfm-host-')
        cls.binary = Path(cls.build.name) / 'modules'
        subprocess.run(['gcc', '-O2', '-c', str(ROOT / 'third_party/miniz/miniz.c'),
                        '-o', str(Path(cls.build.name) / 'miniz.o')], check=True, capture_output=True)
        sources = ['tests/host/modules.cpp', 'src/core/paths.cpp', 'src/storage/filesystem.cpp',
                   'src/security/path_policy.cpp', 'src/storage/operations.cpp', 'src/diagnostics/logger.cpp', 'src/archives/zipx.cpp']
        subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-O2', '-pthread', '-I' + str(ROOT / 'include'),
                        '-I' + str(ROOT / 'third_party/miniz'), *[str(ROOT / p) for p in sources],
                        str(Path(cls.build.name) / 'miniz.o'), '-o', str(cls.binary)], check=True, capture_output=True)

    @classmethod
    def tearDownClass(cls):
        cls.build.cleanup()

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='sfm-data-')
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        (self.root / 'sdmc:').mkdir()

    def run_module(self, *args, success=True):
        p = subprocess.run([str(self.binary), *map(str, args)], cwd=self.root, capture_output=True, text=True, timeout=20)
        if success:
            self.assertEqual(p.returncode, 0, p.stderr)
        else:
            self.assertNotEqual(p.returncode, 0)
        return p

    def test_paths_and_legacy_config(self):
        p = self.run_module('paths')
        self.assertIn('sdmc:/switch/switch-manager', p.stdout)
        self.assertIn('sdmc:/switch/Switch File Manager/logs/sfm.log', p.stdout)

    def log_path(self):
        return self.root / 'sdmc:/switch/Switch File Manager/logs/sfm.log'

    def test_log_creation_append_and_line_safety(self):
        self.run_module('log')
        self.run_module('log')
        lines = self.log_path().read_text().splitlines()
        self.assertEqual(len(lines), 2)
        self.assertIn('[TESTE] mensagem sem linha falsa', lines[0])

    def test_log_concurrent_writes(self):
        self.run_module('parallel')
        lines = self.log_path().read_text().splitlines()
        self.assertEqual(len(lines), 400)
        self.assertEqual(len({x.split('[THREAD] ')[1] for x in lines}), 400)

    def test_log_rotation(self):
        self.run_module('rotate')
        self.assertLess(self.log_path().stat().st_size, 1024 * 1024)
        previous = Path(str(self.log_path()) + '.1')
        self.assertGreaterEqual(previous.stat().st_size, 1024 * 1024)
        self.assertLess(previous.stat().st_size, 1024 * 1024 + 8192)

    def test_log_directory_error(self):
        (self.root / 'sdmc:/switch').write_text('preservar')
        p = self.run_module('log', success=False)
        self.assertIn('errno=', p.stderr)
        self.assertEqual((self.root / 'sdmc:/switch').read_text(), 'preservar')

    @unittest.skipUnless(Path('/dev/full').exists(), 'Requer /dev/full para simular disco cheio')
    def test_log_flush_error(self):
        self.log_path().parent.mkdir(parents=True)
        self.log_path().symlink_to('/dev/full')
        self.run_module('log', success=False)

    def test_zip_roundtrip(self):
        source = self.root / 'origem'
        source.mkdir()
        (source / 'vazio').mkdir()
        data = bytes(range(256)) * 4000
        (source / 'conteúdo.bin').write_bytes(data)
        archive = self.root / 'roundtrip.zip'
        self.run_module('compress', source, archive)
        with zipfile.ZipFile(archive) as z:
            self.assertEqual(z.read('origem/conteúdo.bin'), data)
        self.run_module('inspect', archive)
        dest = self.root / 'extraido'
        self.run_module('extract', archive, dest)
        self.assertEqual((dest / 'origem/conteúdo.bin').read_bytes(), data)
        self.assertTrue((dest / 'origem/vazio').is_dir())

    def test_zip_blocks_traversal(self):
        archive = self.root / 'traversal.zip'
        with zipfile.ZipFile(archive, 'w') as z:
            for name in ('../fora', '/absoluto', 'C:\\arquivo', 'pasta\\..\\fora'):
                z.writestr(name, b'proibido')
            z.writestr('permitido.txt', b'ok')
        dest = self.root / 'destino'
        result = self.run_module('extract', archive, dest)
        self.assertEqual(result.stdout.strip(), '4')
        self.assertEqual(list(dest.iterdir()), [dest / 'permitido.txt'])
        self.assertFalse((self.root / 'fora').exists())

    def test_zip_invalid_and_cancellation(self):
        archive = self.root / 'bad.zip'
        archive.write_bytes(b'invalido')
        self.run_module('inspect', archive, success=False)
        with zipfile.ZipFile(archive, 'w') as z:
            z.writestr('large.bin', b'x' * 600000)
        dest = self.root / 'cancelado'
        self.run_module('cancel-extract', archive, dest, success=False)
        self.assertFalse((dest / 'large.bin').exists())

    @unittest.skipUnless(Path('/dev/full').exists(), 'Requer /dev/full')
    def test_zip_reports_final_flush_failure(self):
        archive = self.root / 'flush.zip'
        with zipfile.ZipFile(archive, 'w') as z:
            z.writestr('arquivo.txt', b'pequeno')
        dest = self.root / 'flush'
        dest.mkdir()
        (dest / 'arquivo.txt').symlink_to('/dev/full')
        self.run_module('extract', archive, dest, success=False)

    def test_local_copy_verify_and_remove_tree(self):
        source = self.root / 'original'
        (source / 'vazio').mkdir(parents=True)
        data = bytes(range(256)) * 10000
        (source / 'arquivo.bin').write_bytes(data)
        destination = self.root / 'copia'
        self.run_module('copy', source, destination)
        self.run_module('verify', source, destination)
        self.assertEqual((destination / 'arquivo.bin').read_bytes(), data)
        (destination / 'arquivo.bin').write_bytes(b'alterado')
        self.run_module('verify', source, destination, success=False)
        self.assertEqual((source / 'arquivo.bin').read_bytes(), data)
        self.run_module('remove', destination)
        self.assertFalse(destination.exists())
        self.assertTrue(source.exists())

    def test_local_copy_preserves_existing_destination(self):
        source = self.root / 'origem.bin'; source.write_bytes(b'origem')
        dest = self.root / 'destino.bin'; dest.write_bytes(b'preservar')
        self.run_module('copy', source, dest, success=False)
        self.assertEqual(dest.read_bytes(), b'preservar')
        self.assertEqual(source.read_bytes(), b'origem')

    def test_local_copy_cancellation_cleans_only_partial(self):
        source = self.root / 'original'
        source.mkdir()
        (source / 'grande.bin').write_bytes(b'x' * 3000000)
        dest = self.root / 'parcial'
        self.run_module('cancel-copy', source, dest, success=False)
        self.assertFalse(dest.exists())
        self.assertEqual((source / 'grande.bin').stat().st_size, 3000000)
