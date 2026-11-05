from pathlib import Path
import errno
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class CredentialStoreTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory(prefix='sfm-persistence-build-')
        cls.binary = Path(cls.build.name) / 'credential-store'
        sources = ['tests/host/credential_store.cpp', 'tests/host/fsdev_rename.cpp',
                   'src/cloud/credential_store.cpp', 'src/diagnostics/logger.cpp']
        subprocess.run(['g++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-pthread', '-DHOST_TEST',
                        '-I' + str(ROOT / 'include'), *[str(ROOT / p) for p in sources],
                        '-Wl,--wrap=rename', '-o', str(cls.binary)], check=True, capture_output=True)

    @classmethod
    def tearDownClass(cls):
        cls.build.cleanup()

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='sfm-persistence-')
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        (self.root / 'sdmc:').mkdir()
        self.config = self.root / 'config'
        self.config.mkdir()
        self.token = self.config / 'token.txt'
        self.token.write_text('fictional-existing')

    def run_store(self, *args, command='save', file='token', success=True, exit_code=None):
        result = subprocess.run([str(self.binary), command, str(self.config), file, *args],
                                cwd=self.root, capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, exit_code if exit_code is not None else (0 if success else 1), result.stderr)
        self.assertNotIn('fictional-existing', result.stdout + result.stderr)
        self.assertNotIn('fictional-refresh-new', result.stdout + result.stderr)
        return result

    def test_replaces_existing_token_under_fsdev_no_overwrite_semantics(self):
        self.run_store()
        self.assertEqual(self.token.read_text(), 'fictional-refresh-new')
        self.assertEqual((self.config / 'token.txt.sfm-old-000').read_text(), 'fictional-existing')

    def test_partial_writes_and_interruptions(self):
        self.run_store('--short', f'gravar-temporario={errno.EINTR}', f'sincronizar-temporario={errno.EINTR}')
        self.assertEqual(self.token.read_text(), 'fictional-refresh-new')

    def test_failure_stages_preserve_original_and_report_errno(self):
        cases = [('abrir-temporario', errno.EROFS), ('gravar-temporario', errno.ENOSPC),
                 ('sincronizar-temporario', errno.EIO), ('fechar-temporario', errno.EIO),
                 ('abrir-leitura-temporario', errno.EACCES), ('ler-temporario', errno.EIO),
                 ('fechar-leitura-temporario', errno.EIO), ('comparar-temporario', errno.EIO),
                 ('mover-anterior', errno.EACCES), ('instalar-novo', errno.EIO),
                 ('ler-final', errno.EIO), ('comparar-final', errno.EIO)]
        for stage, code in cases:
            with self.subTest(stage=stage):
                result = self.run_store(f'{stage}={code}', success=False)
                self.assertIn('etapa=' + stage, result.stdout)
                self.assertIn('errno=' + str(code), result.stdout)
                self.assertEqual(self.token.read_text(), 'fictional-existing')

    def test_zero_write_does_not_confirm_save(self):
        self.run_store('gravar-temporario=0', success=False)
        self.assertEqual(self.token.read_text(), 'fictional-existing')

    def test_unsupported_sync_is_diagnosed_without_deleting_token(self):
        result = self.run_store(f'sincronizar-temporario={errno.ENOSYS}', success=False)
        self.assertIn('sincronizar-temporario', result.stdout)
        self.assertIn('errno=' + str(errno.ENOSYS), result.stdout)
        self.assertEqual(self.token.read_text(), 'fictional-existing')

    def test_recovery_after_failed_install_and_failed_rollback(self):
        result = self.run_store(f'instalar-novo={errno.EIO}', f'restaurar-anterior={errno.EROFS}', success=False)
        self.assertIn('recuperacao=restaurar-anterior', result.stdout)
        self.assertFalse(self.token.exists())
        self.assertEqual((self.config / 'token.txt.sfm-old-000').read_text(), 'fictional-existing')
        self.run_store(command='load')
        self.assertEqual(self.token.read_text(), 'fictional-existing')

    def test_recovery_after_process_stops_between_renames(self):
        self.run_store('--crash=instalar-novo', exit_code=77)
        self.assertFalse(self.token.exists())
        self.run_store(command='load')
        self.assertEqual(self.token.read_text(), 'fictional-existing')

    def test_first_save_failure_keeps_unverified_candidate_out_of_login(self):
        self.token.unlink()  # somente fixture temporária
        self.run_store('comparar-final=' + str(errno.EIO), success=False)
        self.assertFalse(self.token.exists())
        self.run_store(command='load', success=False)
        self.assertTrue(list(self.config.glob('token.txt.sfm-new-*')))

    def test_existing_partial_candidate_is_not_overwritten(self):
        pending = self.config / 'token.txt.sfm-new-000'
        pending.write_text('fictional-partial-to-preserve')
        self.run_store()
        self.assertEqual(pending.read_text(), 'fictional-partial-to-preserve')
        self.assertEqual((self.config / 'token.txt.sfm-old-001').read_text(), 'fictional-existing')

    def test_backup_capacity_fails_without_cleaning_user_data(self):
        backup = self.config / 'token.txt.sfm-old-127'
        backup.write_text('fictional-backup-to-preserve')
        self.run_store(success=False)
        self.assertEqual(self.token.read_text(), 'fictional-existing')
        self.assertEqual(backup.read_text(), 'fictional-backup-to-preserve')

    def test_credential_failure_preserves_existing_credential(self):
        credential = self.config / 'google.json'
        credential.write_text('fictional-existing-credential')
        self.run_store(f'instalar-novo={errno.EIO}', file='credential', success=False)
        self.assertEqual(credential.read_text(), 'fictional-existing-credential')

    def test_directory_creation_and_file_obstruction(self):
        self.config = self.root / 'new' / 'nested' / 'config'
        self.run_store(command='ensure')
        self.assertTrue(self.config.is_dir())
        obstruction = self.root / 'obstruction'
        obstruction.write_text('preservar')
        self.config = obstruction / 'config'
        self.run_store(command='ensure', success=False)
        self.assertEqual(obstruction.read_text(), 'preservar')

    def test_directory_creation_failure_reports_operation(self):
        result = self.run_store(f'criar-diretorio={errno.EROFS}', command='ensure', success=False)
        self.assertIn('etapa=criar-diretorio', result.stdout)
        self.assertEqual(self.token.read_text(), 'fictional-existing')

    def test_unchanged_token_needs_only_verified_read(self):
        self.token.write_text('fictional-refresh-new')
        self.run_store(f'abrir-temporario={errno.EROFS}')
        self.assertFalse(list(self.config.glob('*.sfm-*')))
