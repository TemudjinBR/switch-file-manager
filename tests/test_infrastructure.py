import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from version import read_version
from prepare import prepare
from validate_nro import validate


class InfrastructureTests(unittest.TestCase):
    def test_version(self):
        self.assertRegex(read_version(), r'^\d+\.\d+\.\d+$')
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for bad in ('v1.2.3', '1.2', '01.2.3', '1.2.3\nINJECT=1', '1234567890.12345.6'):
                (root / 'VERSION').write_text(bad)
                with self.assertRaises(ValueError):
                    read_version(root)

    def test_prepare_is_idempotent(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'VERSION').write_text('1.2.3\n')
            p = prepare(root)
            stamp = p.stat().st_mtime_ns
            self.assertEqual(p, prepare(root))
            self.assertEqual(stamp, p.stat().st_mtime_ns)
            self.assertIn('"1.2.3"', p.read_text())
            (root / 'VERSION').write_text('1.2.4\n')
            prepare(root)
            self.assertIn('"1.2.4"', p.read_text())

    def test_rejects_invalid_nro(self):
        with tempfile.TemporaryDirectory() as directory:
            p = Path(directory) / 'bad.nro'
            for data in (b'', b'\0' * 16 + b'NRO0', b'\0' * 200):
                p.write_bytes(data)
                with self.assertRaises(ValueError):
                    validate(p)


if __name__ == '__main__':
    unittest.main()
