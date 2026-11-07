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
from prepare import build_identity
from inspect_installation import inspect


def minimal_nro(root=ROOT, title_name='Switch File Manager'):
    # Fixture de formato, não um programa executável; o teste opcional usa o NRO compilado.
    executable = bytearray(0x3000)
    executable[0x10:0x14] = b'NRO0'
    struct.pack_into('<I', executable, 0x18, len(executable))
    for offset, start in ((0x20, 0), (0x28, 0x1000), (0x30, 0x2000)):
        struct.pack_into('<II', executable, offset, start, 0x1000)
    nacp = bytearray(0x4000)
    title = title_name.encode()
    nacp[:len(title)] = title
    marker = ('SFM_BUILD:' + build_identity(root)).encode() + b'\0'
    executable[0x100:0x100+len(marker)] = marker
    version = read_version(root).encode()
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
        helper = Path(nro).with_name('SfmUpdateHelper.nro')
        if not helper.exists(): helper.write_bytes(minimal_nro(title_name='Atualizador do Switch File Manager'))
        archive = package(nro, directory)
        first = archive.read_bytes()
        self.assertEqual(package(nro, directory).read_bytes(), first)
        digest = hashlib.sha256(Path(nro).read_bytes()).hexdigest()
        with zipfile.ZipFile(archive) as z:
            self.assertEqual(set(z.namelist()), {SD_PATH, 'switch/Switch File Manager/SfmUpdateHelper.nro', 'LEIA-ME.txt', 'manifest.json', 'SHA256SUMS'})
            self.assertEqual(z.read(SD_PATH), Path(nro).read_bytes())
            self.assertEqual(z.read('switch/Switch File Manager/SfmUpdateHelper.nro'), helper.read_bytes())
            manifest = json.loads(z.read('manifest.json'))
            self.assertEqual(manifest['versao'], read_version())
            self.assertEqual(manifest['caminho_instalacao'], SD_PATH)
            instructions = z.read('LEIA-ME.txt').decode()
            self.assertIn(SD_PATH, instructions)
            self.assertIn(read_version(), instructions)
            self.assertIn(digest, instructions)
            self.assertEqual(manifest['nro_sha256'], digest)
            self.assertIn(digest, z.read('SHA256SUMS').decode())
            self.assertIsNone(z.testzip())
        self.assertEqual((Path(directory) / 'SwitchFileManager.nro.sha256').read_text().split()[0], digest)
        self.assertEqual((Path(directory)/'SfmUpdateHelper.nro.sha256').read_text().split()[0],hashlib.sha256(helper.read_bytes()).hexdigest())
        self.assertEqual((Path(directory)/(archive.name+'.sha256')).read_text().split()[0],hashlib.sha256(archive.read_bytes()).hexdigest())

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

    def test_installation_inspector_finds_old_duplicate_without_modifying_it(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            current = root / 'switch/Switch File Manager/SwitchFileManager.nro'
            current.parent.mkdir(parents=True)
            current.write_bytes(minimal_nro())
            old = root / 'switch/SwitchFileManager.nro'
            old_data = bytearray(minimal_nro())
            size = struct.unpack_from('<I', old_data, 0x18)[0]
            nacp_off = struct.unpack_from('<Q', old_data, size + 24)[0]
            start = size + nacp_off + 0x3060
            old_data[start:start+16] = b'0.0.1' + b'\0' * 11
            old.write_bytes(old_data)
            result = inspect(root)
            self.assertTrue(result['duplicado'])
            self.assertEqual(sorted(x['corresponde'] for x in result['copias']), [False, True])
            self.assertEqual(old.read_bytes(), bytes(old_data))
            self.assertEqual(current.read_bytes(), minimal_nro())

    def test_hidden_nro_copies_are_hashed_read_only(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            hidden = root / '.backup/.SwitchFileManager.NRO'
            hidden.parent.mkdir()
            payload = minimal_nro()
            hidden.write_bytes(payload)
            visible = root / 'SwitchFileManager.nro'
            visible.write_bytes(payload)
            before = {p.relative_to(root): p.read_bytes() for p in (hidden, visible)}
            result = inspect(root)
            self.assertTrue(result['duplicado'])
            self.assertEqual(len(result['copias']), 2)
            self.assertTrue(all(item['sha256'] == hashlib.sha256(payload).hexdigest()
                                for item in result['copias']))
            self.assertEqual(before, {p.relative_to(root): p.read_bytes() for p in (hidden, visible)})

    def test_nacp_version_is_extracted_directly_from_compiled_asset(self):
        # A fixture é verificada sempre; o caso compilado também roda quando --nro é fornecido.
        data = Path(os.environ['SFM_TEST_NRO']).read_bytes() if os.environ.get('SFM_TEST_NRO') else minimal_nro()
        size = struct.unpack_from('<I', data, 0x18)[0]
        start, length = struct.unpack_from('<QQ', data, size + 24)
        self.assertEqual(length, 0x4000)
        embedded = data[size+start+0x3060:size+start+0x3070].split(b'\0')[0].decode()
        self.assertEqual(embedded, (ROOT / 'VERSION').read_text().strip())

    def test_distribution_excludes_authentication_and_private_fixture_values(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'VERSION').write_text(read_version())
            secrets = ['fictional-client-secret-never-package', 'fictional-user-refresh-never-package']
            (root / 'google.json').write_text(secrets[0])
            (root / 'token.txt').write_text(secrets[1])
            (root / 'drive-account.json.sfm-old-000').write_text(secrets[1])
            nro = root / 'test.nro'; nro.write_bytes(minimal_nro(root))
            (root / 'SfmUpdateHelper.nro').write_bytes(minimal_nro(root, 'Atualizador do Switch File Manager'))
            archive = package(nro, root / 'dist', root=root)
            with zipfile.ZipFile(archive) as z:
                for name in z.namelist():
                    for secret in secrets:
                        self.assertNotIn(secret.encode(), z.read(name))

    def test_stable_package_channel_is_consistent_without_changing_binary(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);nro=root/'SwitchFileManager.nro';nro.write_bytes(minimal_nro())
            helper=root/'SfmUpdateHelper.nro';helper.write_bytes(minimal_nro(title_name='Atualizador do Switch File Manager'))
            archive=package(nro,root/'dist',channel='stable')
            update=json.loads((root/'dist/update.json').read_text())
            self.assertEqual(update['channel'],'stable')
            self.assertEqual(update['app']['sha256'],hashlib.sha256(nro.read_bytes()).hexdigest())
            self.assertEqual(update['helper']['sha256'],hashlib.sha256(helper.read_bytes()).hexdigest())
            with zipfile.ZipFile(archive) as z:self.assertEqual(json.loads(z.read('manifest.json'))['canal'],'stable')

    @unittest.skipUnless(os.environ.get('SFM_TEST_NRO'), 'Auxiliar nativo disponível após compilar os dois NROs')
    def test_compiled_helper_has_matching_version_build_and_official_icon(self):
        path=Path(os.environ['SFM_TEST_NRO']).with_name('SfmUpdateHelper.nro')
        metadata=validate(path,read_version(),build_identity(),'Atualizador do Switch File Manager')
        self.assertEqual(metadata['versao'],read_version())
        data=path.read_bytes();size=struct.unpack_from('<I',data,0x18)[0]
        offset,length=struct.unpack_from('<QQ',data,size+8)
        self.assertEqual(data[size+offset:size+offset+length],(ROOT/'assets/icon.jpg').read_bytes())
