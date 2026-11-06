from pathlib import Path
import ftplib
import io
import subprocess
import tempfile
import unittest
from urllib.parse import urlparse

ROOT = Path(__file__).resolve().parents[1]


class NetworkLifecycleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory(prefix='sfm-lifecycle-')
        cls.binary = Path(cls.build.name) / 'network-lifecycle'
        sources = ['tests/host/network_lifecycle.cpp', 'src/network/socket_runtime.cpp',
                   'src/network/wifi_transfer.cpp', 'src/cloud/gdrive.cpp', 'src/cloud/download_manager.cpp', 'src/security/oauth.cpp',
                   'src/cloud/credential_store.cpp', 'src/diagnostics/logger.cpp']
        subprocess.run(['g++', '-std=c++17', '-O2', '-pthread', '-DHOST_TEST',
                        '-I' + str(ROOT / 'include'), '-I' + str(ROOT / 'tests/stubs'),
                        *[str(ROOT / s) for s in sources], '-o', str(cls.binary)], check=True, capture_output=True)

    @classmethod
    def tearDownClass(cls):
        cls.build.cleanup()

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='sfm-lifecycle-data-')
        self.addCleanup(self.tmp.cleanup)
        (Path(self.tmp.name) / 'sdmc:').mkdir()
        self.process = subprocess.Popen([self.binary], cwd=self.tmp.name, stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.addCleanup(self.stop)

    def stop(self):
        try:
            _, err = self.process.communicate('quit\n', timeout=10)
        except subprocess.TimeoutExpired:
            self.process.kill(); self.process.communicate(); self.fail('Rede não encerrou')
        self.assertEqual(self.process.returncode, 0, err)

    def command(self, value):
        self.process.stdin.write(value + '\n'); self.process.stdin.flush()
        lines = []
        while True:
            line = self.process.stdout.readline().strip()
            self.assertTrue(line, 'Processo encerrou sem responder')
            lines.append(line)
            if line.startswith('init='):
                return lines

    def transfer(self, url):
        target = urlparse(url)
        with ftplib.FTP() as ftp:
            ftp.connect(target.hostname, target.port, timeout=5); ftp.login()
            data = b'fictional-lifecycle' * 10000
            ftp.storbinary('STOR lifecycle.bin', io.BytesIO(data))
            got = io.BytesIO(); ftp.retrbinary('RETR lifecycle.bin', got.write)
            self.assertEqual(got.getvalue(), data)
            ftp.quit()

    def test_ftp_stop_restart_and_idempotence(self):
        first = self.command('ftp-on'); self.transfer(first[0])
        self.assertEqual(self.command('ftp-on')[-1], 'init=1 exit=0 up=1')
        self.assertEqual(self.command('ftp-off')[-1], 'init=1 exit=1 up=0')
        self.assertEqual(self.command('ftp-off')[-1], 'init=1 exit=1 up=0')
        second = self.command('ftp-on'); self.transfer(second[0])
        self.assertEqual(second[-1], 'init=2 exit=1 up=1')

    def test_drive_then_ftp_shares_runtime_and_survives_drive_exit(self):
        self.command('drive-on'); self.command('drive-on')
        ftp = self.command('ftp-on'); self.assertEqual(ftp[-1], 'init=1 exit=0 up=1')
        self.assertEqual(self.command('drive-off')[-1], 'init=1 exit=0 up=1')
        self.transfer(ftp[0])
        self.assertEqual(self.command('ftp-off')[-1], 'init=1 exit=1 up=0')

    def test_ftp_then_drive_survives_ftp_exit(self):
        self.command('ftp-on'); self.command('drive-on')
        self.assertEqual(self.command('ftp-off')[-1], 'init=1 exit=0 up=1')
        self.assertEqual(self.command('drive-off')[-1], 'init=1 exit=1 up=0')

    def test_failure_records_result_and_allows_retry(self):
        self.command('fail')
        error = self.command('ftp-on')
        self.assertIn('Result=0x00000345', '\n'.join(error))
        self.assertEqual(error[-1], 'init=1 exit=0 up=0')
        retry = self.command('ftp-on'); self.transfer(retry[0])
        self.assertEqual(retry[-1], 'init=2 exit=0 up=1')

    def test_drive_network_failure_is_recoverable(self):
        self.command('fail'); error = self.command('drive-on')
        self.assertIn('drive=0', error[0]); self.assertIn('0x00000345', '\n'.join(error))
        self.assertEqual(self.command('drive-on')[-1], 'init=2 exit=0 up=1')

    def test_wifi_loss_return_and_ftp_restart_preserve_drive_owner(self):
        self.command('drive-on')
        self.command('network-off')
        failed = self.command('ftp-on')
        self.assertIn('etapa=wifi-ip', '\n'.join(failed))
        self.assertEqual(failed[-1], 'init=1 exit=0 up=1')
        self.command('network-on')
        ftp = self.command('ftp-on'); self.transfer(ftp[0])
        self.command('ftp-off')
        self.command('network-off'); self.command('ftp-on')
        self.command('network-on')
        self.transfer(self.command('ftp-on')[0])

    def test_ftp_initialization_stages_are_visible_and_retryable(self):
        for stage in ('criar-socket', 'configurar-socket', 'vincular-2121', 'escutar-2121', 'consultar-porta', 'thread-servidor'):
            with self.subTest(stage=stage):
                self.command('fault-' + stage)
                result = self.command('ftp-on')
                diagnostic = '\n'.join(result)
                self.assertIn('etapa=' + stage, diagnostic)
                self.assertIn('pthread=11' if stage.startswith('thread-') else 'errno=11', diagnostic)
                self.transfer(self.command('ftp-on')[0])
                self.command('ftp-off')

    def test_client_thread_failure_is_visible_and_server_can_accept_next_client(self):
        ftp = self.command('ftp-on')
        self.command('fault-thread-cliente')
        target = urlparse(ftp[0])
        client = ftplib.FTP()
        try:
            with self.assertRaises(ftplib.error_temp):
                client.connect(target.hostname, target.port, timeout=5)
        finally:
            client.close()
        diagnostic = '\n'.join(self.command('diagnostic'))
        self.assertIn('etapa=thread-cliente pthread=11', diagnostic)
        self.transfer(ftp[0])

    def test_session_budget_and_explicit_resource_fallback(self):
        self.command('ftp-on')
        self.assertIn('sessions=8', '\n'.join(self.command('session-count')))
        self.command('ftp-off'); self.command('resource-fail')
        started=self.command('ftp-on'); self.transfer(started[0])
        diagnostic='\n'.join(self.command('session-count'))
        self.assertIn('sessions=3',diagnostic)
        # localAddress atualiza a etapa, mas o orçamento efetivo continua visível.
        self.assertIn('bsd_sessoes=3',diagnostic)
        self.command('drive-on'); self.command('ftp-off')
        self.assertEqual(self.command('drive-off')[-1],'init=3 exit=2 up=0')
