from pathlib import Path
import hashlib
import json
import os
import struct
import sys
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from package import package, SD_PATH
from validate_nro import validate
from version import read_version


def minimal_nro():
    # Fixture de formato, não um programa executável; o teste opcional usa o NRO compilado.
    executable = bytearray(0x3000)
    executable[0x10:0x14] = b'NRO0'
    struct.pack_into('<I', executable, 0x18, len(executable))
    for offset, start in ((0x20, 0), (0x28, 0x1000), (0x30, 0x2000)):
        struct.pack_into('<II', executable, offset, start, 0x1000)
    nacp = bytearray(0x4000)
    title = b'Switch File Manager'
    nacp[:len(title)] = title
    version = read_version().encode()
    nacp[0x3060:0x3060 + len(version)] = version
    aset = bytearray(0x38)
    aset[:4] = b'ASET'
    icon = b'\xff\xd8\xff\xd9'
    struct.pack_into('<QQQQ', aset, 8, len(aset), len(icon), len(aset) + len(icon), len(nacp))
    return bytes(executable + aset + icon + nacp)


class DistributionTests(unittest.TestCase):
    def test_metadata_and_invalid_segments(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'format.nro'
            original = minimal_nro()
            path.write_bytes(original)
            self.assertEqual(validate(path, read_version())['versao'], read_version())
            for offset, value in ((0x18, 0xffffffff), (0x28, 0x5000), (0x30, 0x1000)):
                bad = bytearray(original)
                struct.pack_into('<I', bad, offset, value)
                path.write_bytes(bad)
                with self.assertRaises(ValueError): validate(path, read_version())
            path.write_bytes(original[:-100])
            with self.assertRaises(ValueError): validate(path)

    def test_wrong_nacp_version_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'format.nro'
            path.write_bytes(minimal_nro())
            with self.assertRaises(ValueError): validate(path, '99.99.99')

    def check_package(self, nro, directory):
        archive = package(nro, directory)
        first = archive.read_bytes()
        self.assertEqual(package(nro, directory).read_bytes(), first)
        digest = hashlib.sha256(Path(nro).read_bytes()).hexdigest()
        with zipfile.ZipFile(archive) as z:
            self.assertEqual(set(z.namelist()), {SD_PATH, 'LEIA-ME.txt', 'manifest.json', 'SHA256SUMS'})
            self.assertEqual(z.read(SD_PATH), Path(nro).read_bytes())
            manifest = json.loads(z.read('manifest.json'))
            self.assertEqual(manifest['versao'], read_version())
            self.assertEqual(manifest['nro_sha256'], digest)
            self.assertIn(digest, z.read('SHA256SUMS').decode())
            self.assertIsNone(z.testzip())
        self.assertEqual((Path(directory) / 'SwitchFileManager.nro.sha256').read_text().split()[0], digest)

    def test_distribution_contents_and_determinism(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            nro = root / 'fixture.nro'; nro.write_bytes(minimal_nro())
            self.check_package(nro, root / 'dist')

    @unittest.skipUnless(os.environ.get('SFM_TEST_NRO'), 'Execute com --nro após compilar com devkitPro')
    def test_compiled_nro_and_distribution(self):
        nro = Path(os.environ['SFM_TEST_NRO'])
        validate(nro, read_version())
        with tempfile.TemporaryDirectory() as directory:
            self.check_package(nro, directory)
