from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]


class IoWaitTests(unittest.TestCase):
    def test_nonblocking_tls_wait_limits_and_preserves_certificate_failures(self):
        with tempfile.TemporaryDirectory() as directory:
            binary=Path(directory)/'io-wait'
            subprocess.run(['g++','-std=c++17','-O2','-pthread','-I'+str(ROOT/'include'),str(ROOT/'tests/host/io_wait.cpp'),'-o',str(binary)],check=True,capture_output=True)
            subprocess.run([binary],check=True,timeout=5)
