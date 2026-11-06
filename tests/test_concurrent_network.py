"""Rede POSIX real e fixtures HTTP: sobreposição verificada por barreiras, sem SDK/credenciais reais."""
from pathlib import Path
import ftplib
import io
import json
import os
import queue
import re
import subprocess
import tempfile
import threading
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse

ROOT = Path(__file__).resolve().parents[1]


class ConcurrentNetworkTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory(prefix='sfm-concurrent-build-')
        cls.binary = Path(cls.build.name) / 'concurrent'
        files = ['tests/host/concurrent.cpp', 'src/network/socket_runtime.cpp', 'src/network/wifi_transfer.cpp',
                 'src/cloud/gdrive.cpp', 'src/cloud/download_manager.cpp', 'src/security/oauth.cpp',
                 'src/cloud/credential_store.cpp', 'src/diagnostics/logger.cpp', 'tests/host/fsdev_rename.cpp']
        sanitizer = ['-fsanitize=thread', '-g', '-fno-omit-frame-pointer'] if os.environ.get('SFM_TEST_TSAN') else []
        result = subprocess.run(['g++', *sanitizer, '-std=c++17', '-O2', '-pthread', '-DHOST_TEST',
                                 '-I'+str(ROOT/'include'), '-I'+str(ROOT/'tests/stubs'),
                                 *[str(ROOT/p) for p in files], '-Wl,--wrap=rename', '-o', str(cls.binary)], capture_output=True, text=True)
        if result.returncode: raise RuntimeError(result.stderr)

    @classmethod
    def tearDownClass(cls): cls.build.cleanup()

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='sfm-concurrent-data-'); self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name); (self.root/'sdmc:').mkdir()
        config = self.root/'legacy'; config.mkdir()
        (config/'google.json').write_text(json.dumps({'installed': {'client_id':'fictional-id','client_secret':'fictional-secret'}}))
        (config/'token.txt').write_text('fictional-refresh')
        self.entered = threading.Event(); self.release = threading.Event(); self.mode = 'none'
        self.data = bytes(range(251)) * 8500  # >2 MiB; upload passa por mais de um PUT.
        self.upload = bytearray(); self.ranges = []
        owner = self
        class Handler(BaseHTTPRequestHandler):
            protocol_version = 'HTTP/1.1'
            def log_message(self, *args): pass
            def reply(self, code, body=b'', headers=None):
                if isinstance(body, dict): body = json.dumps(body).encode()
                self.send_response(code); self.send_header('Content-Length', str(len(body)))
                self.send_header('Connection', 'close')
                for key,value in (headers or {}).items(): self.send_header(key,value)
                self.end_headers()
                try: self.wfile.write(body)
                except (BrokenPipeError, ConnectionResetError): pass
            def gate(self):
                owner.entered.set()
                if not owner.release.wait(12): raise RuntimeError('Barreira HTTP expirou')
            def do_POST(self):
                self.rfile.read(int(self.headers.get('Content-Length',0)))
                if self.path == '/token': self.reply(200,{'access_token':'fictional-access','refresh_token':'fictional-refresh','expires_in':3600})
                else: self.reply(200,{}, {'Location':'https://www.googleapis.com/session/test'})
            def do_GET(self):
                if '/about?' in self.path: self.reply(503,{}); return
                if 'alt=media' in self.path:
                    match = re.fullmatch(r'bytes=(\d+)-(\d+)',self.headers.get('Range',''))
                    first,last = map(int,match.groups()); last=min(last,len(owner.data)-1)
                    owner.ranges.append((first,last)); payload=owner.data[first:last+1]
                    self.send_response(206); self.send_header('Content-Length',str(len(payload)))
                    self.send_header('Content-Range',f'bytes {first}-{last}/{len(owner.data)}'); self.send_header('ETag','"fixture-1"')
                    self.send_header('Connection','close'); self.end_headers()
                    try:
                        self.wfile.write(payload[:65536]); self.wfile.flush()
                        if owner.mode == 'download': self.gate()
                        self.wfile.write(payload[65536:])
                    except (BrokenPipeError,ConnectionResetError): pass
                elif '/files/test-file?' in self.path:
                    self.reply(200,{'id':'test-file','size':str(len(owner.data)),'version':'1'})
                else:
                    if owner.mode == 'list': self.gate()
                    self.reply(200,{'files':[{'id':'test-file','name':'fixture.bin','size':str(len(owner.data)),'mimeType':'application/octet-stream'}]})
            def do_PUT(self):
                length=int(self.headers.get('Content-Length',0))
                if owner.mode == 'upload' and not owner.entered.is_set(): self.gate()
                body=self.rfile.read(length); owner.upload.extend(body)
                first,last,total = map(int,re.fullmatch(r'bytes (\d+)-(\d+)/(\d+)',self.headers['Content-Range']).groups())
                self.reply(200 if last+1 == total else 308, {'id':'uploaded'} if last+1 == total else b'', {'Range':f'bytes=0-{last}'})
        self.server = ThreadingHTTPServer(('127.0.0.1',0),Handler)
        self.worker=threading.Thread(target=self.server.serve_forever,daemon=True); self.worker.start()
        self.process=None
        self.addCleanup(self.close_all)
        self.config=config

    def close_all(self):
        self.release.set()
        if self.process:
            try: _,err=self.process.communicate('quit\n',timeout=15)
            except subprocess.TimeoutExpired:
                self.process.kill(); self.process.communicate(); self.fail('Deadlock/encerramento excedeu 15s')
            self.assertEqual(self.process.returncode,0,err)
        self.server.shutdown(); self.server.server_close(); self.worker.join()

    def start(self, order='ftp-first'):
        self.lines=queue.Queue()
        self.process=subprocess.Popen([str(self.binary),str(self.server.server_port),str(self.config),order],cwd=self.root,
                                      stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,bufsize=1)
        def reader():
            for line in self.process.stdout: self.lines.put(line.strip())
        threading.Thread(target=reader,daemon=True).start()
        ready=self.line('READY '); self.assertIn('sessions=8',ready)
        self.url=ready.split()[1]

    def line(self, prefix):
        try: value=self.lines.get(timeout=10)
        except queue.Empty: self.fail('Ausência de resposta/possível deadlock: '+prefix)
        self.assertTrue(value.startswith(prefix),value)
        return value

    def send(self, command):
        self.process.stdin.write(command+'\n'); self.process.stdin.flush()

    def ftp_roundtrip(self):
        address=urlparse(self.url)
        with ftplib.FTP() as ftp:
            ftp.connect(address.hostname,address.port,timeout=5); ftp.login()
            data=b'FTP simultaneous fixture' * 60000
            ftp.storbinary('STOR ftp.bin',io.BytesIO(data))
            received=io.BytesIO(); ftp.retrbinary('RETR ftp.bin',received.write)
            self.assertEqual(received.getvalue(),data)
            ftp.quit()
        self.assertEqual((self.root/'sdmc:/ftp.bin').read_bytes(),data)

    def overlap(self, mode, order='ftp-first'):
        self.start(order); self.mode=mode
        if mode=='upload':
            source=self.root/'upload.bin'; source.write_bytes(self.data); command='upload='+str(source)
        elif mode=='download': command='download='+str(self.root/'download.bin')
        else: command='list'
        self.send(command); self.assertTrue(self.entered.wait(5),'Drive não atingiu a barreira')
        self.ftp_roundtrip()  # Só libera Drive DEPOIS de verificar os bytes FTP.
        self.assertFalse(self.release.is_set()); self.release.set()
        self.assertIn('ok=1 logged=1',self.line('DONE '))
        if mode=='download': self.assertEqual((self.root/'download.bin').read_bytes(),self.data)
        if mode=='upload': self.assertEqual(bytes(self.upload),self.data)
        self.ftp_roundtrip()  # Drive terminou; FTP continua ativo.

    def test_ftp_during_drive_navigation_in_same_process(self): self.overlap('list')
    def test_ftp_upload_download_during_drive_download(self): self.overlap('download')
    def test_drive_first_then_concurrent_upload_and_ftp(self): self.overlap('upload','drive-first')

    def test_ftp_restart_while_drive_response_is_pending(self):
        self.start(); self.mode='list'; self.send('list'); self.assertTrue(self.entered.wait(5))
        self.send('ftp-stop'); self.assertIn('ftp=0',self.line('LIVE ftp-stop'))
        self.send('ftp-start'); response=self.line('LIVE ftp-start'); self.assertIn('ftp=1',response)
        self.url=response.split('url=')[1].split()[0]; self.ftp_roundtrip()
        self.release.set(); self.assertIn('ok=1 logged=1',self.line('DONE '))

    def test_drive_stop_during_request_preserves_ftp_and_can_reconnect(self):
        self.start('drive-first'); self.mode='list'; self.send('list'); self.assertTrue(self.entered.wait(5))
        self.send('drive-stop'); self.assertIn('ftp=1',self.line('LIVE drive-stop'))
        self.assertIn('ok=0',self.line('DONE ')); self.ftp_roundtrip()
        self.release.set(); self.mode='none'; self.send('list')
        self.assertIn('ok=1 logged=1',self.line('DONE ')); self.ftp_roundtrip()

    def test_four_idle_clients_do_not_block_drive_request(self):
        self.start(); clients=[]
        try:
            for _ in range(4):
                ftp=ftplib.FTP(); address=urlparse(self.url)
                ftp.connect(address.hostname,address.port,timeout=5); ftp.login(); clients.append(ftp)
            self.send('list'); self.assertIn('ok=1 logged=1',self.line('DONE '))
            for ftp in clients: self.assertTrue(ftp.voidcmd('NOOP').startswith('200'))
        finally:
            for ftp in clients: ftp.close()

    def test_four_parallel_ftp_transfers_while_drive_download_waits(self):
        self.start(); self.mode='download'; self.send('download='+str(self.root/'download.bin'))
        self.assertTrue(self.entered.wait(5)); failures=[]
        def transfer(index):
            try:
                address=urlparse(self.url)
                with ftplib.FTP() as ftp:
                    ftp.connect(address.hostname,address.port,timeout=5); ftp.login()
                    data=bytes([index])*600000; name=f'parallel-{index}.bin'
                    ftp.storbinary('STOR '+name,io.BytesIO(data))
                    got=io.BytesIO(); ftp.retrbinary('RETR '+name,got.write)
                    if got.getvalue()!=data: raise AssertionError('Conteúdo FTP divergente')
                    ftp.quit()
            except Exception as error: failures.append(repr(error))
        workers=[threading.Thread(target=transfer,args=(i,)) for i in range(4)]
        for worker in workers: worker.start()
        for worker in workers: worker.join(8); self.assertFalse(worker.is_alive(),'Possível deadlock FTP concorrente')
        self.assertEqual(failures,[]); self.assertFalse(self.release.is_set())
        self.release.set(); self.assertIn('ok=1 logged=1',self.line('DONE '))
        self.assertEqual((self.root/'download.bin').read_bytes(),self.data)

    def test_stopping_busy_ftp_client_does_not_close_drive_descriptor(self):
        self.start(); self.mode='list'; self.send('list'); self.assertTrue(self.entered.wait(5))
        address=urlparse(self.url)
        with ftplib.FTP() as ftp:
            ftp.connect(address.hostname,address.port,timeout=5); ftp.login()
            channel=ftp.transfercmd('STOR unfinished.bin')
            channel.sendall(b'partial fixture')
            self.send('ftp-stop'); self.assertIn('ftp=0',self.line('LIVE ftp-stop')); channel.close()
        self.assertFalse((self.root/'sdmc:/unfinished.bin').exists())
        self.release.set(); self.assertIn('ok=1 logged=1',self.line('DONE '))
        self.send('ftp-start'); self.url=self.line('LIVE ftp-start').split('url=')[1].split()[0]
        self.ftp_roundtrip()

    def test_stalled_drive_body_times_out_without_stopping_ftp(self):
        self.start(); self.mode='list'; self.send('list'); self.assertTrue(self.entered.wait(5))
        self.assertIn('ok=0 logged=1',self.line('DONE '))
        self.ftp_roundtrip(); self.release.set(); self.mode='none'; self.send('list')
        self.assertIn('ok=1 logged=1',self.line('DONE '))
