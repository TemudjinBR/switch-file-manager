from pathlib import Path
import os
import subprocess
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from prepare import build_identity, prepare
from test_distribution import minimal_nro
from validate_nro import validate


class BuildInfoTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build=tempfile.TemporaryDirectory(prefix='sfm-build-info-')
        cls.binary=Path(cls.build.name)/'build-info'
        prepare()
        subprocess.run(['g++','-std=c++17','-O2','-I'+str(ROOT/'include'),'-I'+str(ROOT/'build/generated'),
                        str(ROOT/'tests/host/build_info.cpp'),str(ROOT/'src/diagnostics/build_info.cpp'),
                        '-o',str(cls.binary)],check=True,capture_output=True)

    @classmethod
    def tearDownClass(cls): cls.build.cleanup()

    def test_compiled_identity_and_read_only_duplicate_scan(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory); current=root/'current.nro';current.write_bytes(minimal_nro())
            old=root/'old.nro';data=bytearray(minimal_nro())
            exe=struct.unpack_from('<I',data,0x18)[0];off=struct.unpack_from('<Q',data,exe+24)[0]
            data[exe+off+0x3060:exe+off+0x3070]=b'0.10.8'+b'\0'*10
            old.write_bytes(data)
            out=subprocess.check_output([self.binary,root],text=True)
            self.assertIn('SFM_BUILD:'+build_identity(),out)
            self.assertIn('|0.10.8|',out);self.assertIn('current.nro|',out)
            self.assertEqual(old.read_bytes(),bytes(data));self.assertEqual(current.read_bytes(),minimal_nro())

    def test_scan_ignores_symlinks_and_invalid_assets(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            (root/'outside.nro').symlink_to(ROOT/'SwitchFileManager.nro')
            (root/'invalid.nro').write_bytes(b'NRO0invalid')
            out=subprocess.check_output([self.binary,root],text=True)
            self.assertNotIn('outside.nro|',out);self.assertNotIn('invalid.nro|',out)

    def test_scan_can_be_cancelled_without_mutation(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/'test.nro').write_bytes(minimal_nro())
            before=(root/'test.nro').read_bytes()
            out=subprocess.check_output([self.binary,root,'cancel'],text=True)
            self.assertIn('cancelled=1',out)
            self.assertEqual((root/'test.nro').read_bytes(),before)

    def test_validator_rejects_other_build_with_same_version(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'test.nro';path.write_bytes(minimal_nro())
            with self.assertRaises(ValueError):validate(path,expected_build='v0.10.0-000000000000')

    def test_build_id_changes_for_sources_and_ignores_documentation(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/'VERSION').write_text('1.2.3')
            (root/'src').mkdir();source=root/'src/app.cpp';source.write_text('int a;')
            first=build_identity(root);(root/'README.md').write_text('documentation')
            self.assertEqual(build_identity(root),first)
            source.write_text('int b;');self.assertNotEqual(build_identity(root),first)
            prepare(root);self.assertIn(build_identity(root),(root/'build/generated/version.h').read_text())
