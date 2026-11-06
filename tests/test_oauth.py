from pathlib import Path
import base64
import hashlib
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class OAuthTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory(prefix='sfm-oauth-')
        cls.binary = Path(cls.build.name) / 'oauth'
        subprocess.run(['g++', '-std=c++17', '-O2', '-DHOST_TEST', '-I' + str(ROOT / 'include'),
                        str(ROOT / 'tests/host/oauth.cpp'), str(ROOT / 'src/security/oauth.cpp'),
                        '-o', str(cls.binary)], check=True, capture_output=True)

    @classmethod
    def tearDownClass(cls):
        cls.build.cleanup()

    def test_pkce_rfc7636_vector_and_sha256_padding(self):
        vector = 'dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk'
        for value in (vector, '', 'a', 'x' * 55, 'x' * 56, 'x' * 64, 'x' * 128):
            with self.subTest(length=len(value)):
                actual = subprocess.check_output([self.binary, value], text=True)
                expected = base64.urlsafe_b64encode(hashlib.sha256(value.encode()).digest()).decode().rstrip('=')
                self.assertEqual(actual, expected)
                if value == vector: self.assertEqual(actual, 'E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM')

    def test_random_nonce_uses_full_entropy_and_url_safe_alphabet(self):
        values = [subprocess.check_output([self.binary], text=True) for _ in range(10)]
        self.assertEqual(len(set(values)), 10)
        for value in values:
            self.assertRegex(value, r'^[A-Za-z0-9_-]{43}$')
