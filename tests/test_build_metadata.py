from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class BuildMetadataTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='sfm-metadata-')
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        (self.root / 'scripts').mkdir()
        shutil.copyfile(ROOT / 'scripts/version.py', self.root / 'scripts/version.py')
        (self.root / 'VERSION').write_text('1.2.3\n')
        (self.root / 'build').mkdir()
        (self.root / 'sdk/libnx').mkdir(parents=True)
        (self.root / 'sdk/libnx/switch_rules').touch()
        (self.root / 'bin').mkdir()
        # Fronteira do SDK isolada: só a geração NACP é simulada. O Makefile real é executado.
        tool = self.root / 'bin/nacptool'
        tool.write_text('#!' + sys.executable + '\n' + '''import sys
from pathlib import Path
_, command, title, author, version, target = sys.argv
assert command == '--create'
nacp = bytearray(0x4000)
nacp[:len(title)] = title.encode()
nacp[0x3060:0x3060+len(version)] = version.encode()
Path(target).write_bytes(nacp)
''')
        tool.chmod(0o755)
        self.env = dict(os.environ, PATH=str(self.root / 'bin') + os.pathsep + os.environ['PATH'])
        self.target = self.root / 'SwitchFileManager.nacp'

    def make(self, *args, ok=True):
        result = subprocess.run(['make', '--no-print-directory', '-f', str(ROOT / 'Makefile'),
                                 'DEVKITPRO=' + str(self.root / 'sdk'), 'TOPDIR=' + str(self.root),
                                 'OUTPUT=' + str(self.root / 'SwitchFileManager'), *args, str(self.target)],
                                cwd=self.root / 'build', env=self.env, capture_output=True, text=True)
        self.assertEqual(result.returncode == 0, ok, result.stderr)
        return result

    def version(self):
        return self.target.read_bytes()[0x3060:0x3070].split(b'\0')[0].decode()

    def test_stale_external_app_version_cannot_override_project_version(self):
        self.make('APP_VERSION=0.10.8')
        self.assertEqual(self.version(), '1.2.3')

    def test_stale_nacp_with_newer_timestamp_is_rebuilt_and_then_idempotent(self):
        self.make()
        data = bytearray(self.target.read_bytes())
        data[0x3060:0x3070] = b'0.0.1' + b'\0' * 11
        self.target.write_bytes(data)
        os.utime(self.target, (2000000000, 2000000000))
        self.make()
        self.assertEqual(self.version(), '1.2.3')
        timestamp = self.target.stat().st_mtime_ns
        self.make()
        self.assertEqual(self.target.stat().st_mtime_ns, timestamp)

    def test_invalid_version_aborts_instead_of_using_sdk_fallback(self):
        (self.root / 'VERSION').write_text('invalid')
        self.make(ok=False)
        self.assertFalse(self.target.exists())
