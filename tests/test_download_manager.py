from pathlib import Path
import subprocess
import base64
import hashlib
import struct
import zlib
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def expected(changed=False):
    return bytes((i * 17 + (7 if changed else 3)) % 251 for i in range(250000))


class DownloadManagerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory(prefix='sfm-download-build-')
        cls.binary = Path(cls.build.name) / 'downloads'
        files = ['tests/host/downloads.cpp', 'src/cloud/download_manager.cpp', 'src/cloud/credential_store.cpp',
                 'src/diagnostics/logger.cpp', 'src/security/oauth.cpp', 'tests/host/fsdev_rename.cpp']
        subprocess.run(['g++','-std=c++17','-O2','-Wall','-Wextra','-pthread','-DHOST_TEST','-I'+str(ROOT/'include'),
                        *[str(ROOT/p) for p in files],'-Wl,--wrap=rename','-o',str(cls.binary)],check=True,capture_output=True)

    @classmethod
    def tearDownClass(cls): cls.build.cleanup()

    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='sfm-download-data-');self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name);(self.root/'sdmc:').mkdir()
        self.area=self.root/'technical';self.dest=self.root/'personal'/'download.bin';self.dest.parent.mkdir()

    def run_job(self, mode='normal', hook=None, code=0, command='run', dest=None):
        args=[str(self.binary),command,str(self.area),str(dest or self.dest),mode]
        if hook:args.append(hook)
        result=subprocess.run(args,cwd=self.root,capture_output=True,text=True,timeout=15)
        self.assertEqual(result.returncode,code,result.stderr+'\n'+result.stdout[-2000:])
        return result

    def test_normal_chunks_publish_only_complete_file(self):
        result=self.run_job();self.assertEqual(self.dest.read_bytes(),expected())
        self.assertIn('state=Verificando',result.stdout);self.assertIn('state=Concluido',result.stdout)

    def test_disconnect_resumes_from_validated_prefix(self):
        result=self.run_job('cut');self.assertEqual(self.dest.read_bytes(),expected())
        self.assertIn('range=17000-',result.stdout);self.assertIn('range=34000-',result.stdout)
        self.assertIn('state=Reconectando',result.stdout)

    def test_range_ignored_restarts_without_joining_contents(self):
        result=self.run_job('no-range');self.assertEqual(self.dest.read_bytes(),expected())
        self.assertIn('state=Reiniciando',result.stdout)
        self.assertGreaterEqual(len(list(self.area.glob('*/partial-*'))),2)

    def test_remote_version_change_isolates_old_data(self):
        self.run_job('change');self.assertEqual(self.dest.read_bytes(),expected(True))
        self.assertGreaterEqual(len(list(self.area.glob('*/partial-*'))),2)

    def test_retry_limit_preserves_partial_and_recovery_on_next_process(self):
        result=self.run_job('always-cut',code=1);self.assertFalse(self.dest.exists())
        self.assertEqual(result.stdout.count('range='),4)
        resumed=self.run_job();self.assertNotIn('range=0-',resumed.stdout)
        self.assertEqual(self.dest.read_bytes(),expected())

    def test_offline_before_first_byte_remains_recoverable(self):
        self.run_job('offline',code=1);self.assertFalse(self.dest.exists())
        self.run_job();self.assertEqual(self.dest.read_bytes(),expected())

    def test_pause_continue_uses_same_task_and_stops_network_while_paused(self):
        result=self.run_job('pause');self.assertEqual(self.dest.read_bytes(),expected())
        self.assertGreaterEqual(result.stdout.count('state=Pausado'),3)

    def test_cancel_preserves_personal_files_and_other_downloads(self):
        personal=self.dest.parent/'keep.txt';personal.write_bytes(b'personal-fixture')
        other=self.dest.parent/'other.bin'
        self.run_job('always-cut',command='other',dest=other,code=1)
        before={str(p):p.read_bytes() for p in self.area.glob('*/transfer.bin')}
        self.run_job('cancel',code=1);self.assertFalse(self.dest.exists())
        self.assertEqual(personal.read_bytes(),b'personal-fixture')
        self.assertTrue(all(Path(p).read_bytes()==data for p,data in before.items()))
        self.run_job();self.assertEqual(self.dest.read_bytes(),expected())

    def test_process_restart_after_checkpoint_validates_crc_and_resumes(self):
        self.run_job(hook='crash=apos-checkpoint',code=77)
        self.assertFalse(self.dest.exists())
        result=self.run_job();self.assertIn('range=65536-',result.stdout)
        self.assertEqual(self.dest.read_bytes(),expected())

    def test_process_restart_while_paused_keeps_journal(self):
        self.run_job('pause',hook='crash=pausado',code=77)
        listing=self.run_job(command='list');self.assertIn('Pausado',listing.stdout)
        self.run_job();self.assertEqual(self.dest.read_bytes(),expected())

    def test_corrupted_partial_is_preserved_and_not_published(self):
        self.run_job(hook='crash=apos-checkpoint',code=77)
        part=max(self.area.glob('*/partial-*'),key=lambda p:p.stat().st_size)
        original=part.read_bytes();part.write_bytes(b'x'+original[1:])
        result=self.run_job(code=1);self.assertIn('checksum-parcial',result.stderr)
        self.assertFalse(self.dest.exists());self.assertEqual(part.read_bytes(),b'x'+original[1:])

    def test_existing_destination_never_overwritten(self):
        self.dest.write_bytes(b'personal-existing')
        self.run_job(code=1);self.assertEqual(self.dest.read_bytes(),b'personal-existing')

    def test_crash_after_publication_recovers_completed_file(self):
        self.run_job(hook='crash=apos-publicar',code=77)
        self.assertEqual(self.dest.read_bytes(),expected())
        self.run_job();self.assertEqual(self.dest.read_bytes(),expected())

    def test_storage_failures_preserve_destination_and_recover(self):
        for stage in ('gravar-parcial','sincronizar-parcial','publicar-destino'):
            with self.subTest(stage=stage):
                # Um destino diferente por falha, mesma area segura.
                target=self.dest.parent/(stage+'.bin')
                self.run_job(hook='fail='+stage,dest=target,code=1)
                self.assertFalse(target.exists())
                self.run_job(dest=target);self.assertEqual(target.read_bytes(),expected())

    def test_unowned_temporary_directory_is_not_adopted(self):
        self.area.mkdir();(self.area/'keep.txt').write_text('personal')
        self.run_job(code=1)
        self.assertEqual((self.area/'keep.txt').read_text(),'personal')
        self.assertEqual(sorted(p.name for p in self.area.iterdir()),['keep.txt'])

    def test_invalid_range_keeps_partial_and_no_destination(self):
        self.run_job('bad-range',code=1);self.assertFalse(self.dest.exists())
        self.run_job();self.assertEqual(self.dest.read_bytes(),expected())

    def test_connection_loss_keeps_prefix_even_when_metadata_is_offline(self):
        self.run_job('cut-offline', code=1)
        listing = self.run_job(command='list')
        self.assertIn('offset=17000', listing.stdout)
        resumed = self.run_job()
        self.assertIn('range=17000-', resumed.stdout)
        self.assertEqual(self.dest.read_bytes(), expected())

    def test_concurrent_destination_is_not_overwritten_at_publication(self):
        self.run_job('concurrent', code=1)
        self.assertEqual(self.dest.read_bytes(), b'personal-concurrent')

    def test_no_space_preserves_checkpoint_and_can_resume(self):
        self.run_job(hook='crash=apos-checkpoint',code=77)
        before={str(p):p.read_bytes() for p in self.area.glob('*/partial-*')}
        result=self.run_job(hook='fail=espaco-livre',code=1)
        self.assertIn('stage=espaco-livre code=28',result.stderr)
        self.assertNotIn('range=',result.stdout)
        self.assertFalse(self.dest.exists())
        self.assertTrue(all(Path(p).read_bytes()==data for p,data in before.items()))
        self.assertIn('range=65536-',self.run_job().stdout)
        self.assertEqual(self.dest.read_bytes(),expected())

    def test_restart_requires_observer_consent_and_keeps_previous_generation(self):
        self.run_job(hook='crash=apos-checkpoint',code=77)
        journal=next(self.area.glob('*/transfer.bin'))
        old=journal.read_bytes()
        # Modifica a versão remota na fixture: não é alteração de dados pessoais.
        old_fields=[]; cursor=7
        for _ in range(5):
            length=struct.unpack_from('<Q',old,cursor)[0]; cursor+=8
            old_fields.append(old[cursor:cursor+length]); cursor+=length
        old_fields[3]=b'older-version'
        changed=b'SFMDL2\n'+b''.join(struct.pack('<Q',len(x))+x for x in old_fields)+old[cursor:-8]
        changed+=struct.pack('<Q',zlib.crc32(changed))
        journal.write_bytes(changed)
        parts={p:p.read_bytes() for p in self.area.glob('*/partial-*')}
        result=self.run_job('decline-restart',code=1)
        self.assertIn('reinicio-adiado',result.stderr)
        self.assertEqual(journal.read_bytes(),changed)
        self.assertEqual({p:p.read_bytes() for p in self.area.glob('*/partial-*')},parts)
        self.assertFalse(self.dest.exists())
        self.run_job(); self.assertEqual(self.dest.read_bytes(),expected())

    def test_two_gib_file_resumes_at_validated_one_point_two_gib_prefix(self):
        # Sparse file: exercita offsets/CRC reais sem gravar 1,2 GiB nem baixar 2 GiB.
        length=(12*(1<<30))//10
        def field(value): return struct.pack('<Q',len(value))+value
        owner=b'fixture-owner'; identity=b'fixture-id'; dest=str(self.dest).encode()
        key=base64.urlsafe_b64encode(hashlib.sha256(field(owner)+field(identity)+field(dest)).digest()).rstrip(b'=').decode()
        self.area.mkdir(); (self.area/'area.txt').write_bytes(b'SFM_DOWNLOAD_AREA_1\n')
        directory=self.area/key; directory.mkdir()
        part=directory/'partial-0'
        with part.open('wb') as output: output.truncate(length)
        checksum=0; remaining=length; zeros=bytes(1<<20)
        while remaining:
            count=min(remaining,len(zeros)); checksum=zlib.crc32(zeros[:count],checksum); remaining-=count
        job=b'SFMDL2\n'+b''.join(map(field,[owner,identity,dest,b'1',b'']))
        job+=struct.pack('<5Q',2<<30,length,0,checksum,1)
        job+=struct.pack('<Q',zlib.crc32(job)); (directory/'transfer.bin').write_bytes(job)
        result=self.run_job('large-prefix',code=1)
        self.assertIn('range='+str(length)+'-',result.stdout)
        self.assertFalse(self.dest.exists()); self.assertEqual(part.stat().st_size,length)
