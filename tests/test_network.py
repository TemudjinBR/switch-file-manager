from pathlib import Path
import contextlib
import re
import html
import base64
import hashlib
import ftplib
import errno
import http.client
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import io
import json
import select
import socket
import re
import subprocess
import tempfile
import threading
import unittest
from urllib.parse import urlparse, parse_qs, urlencode

ROOT = Path(__file__).resolve().parents[1]


class NetworkTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory(prefix='sfm-network-')
        cls.ftp_binary = Path(cls.build.name) / 'ftp'
        cls.drive_binary = Path(cls.build.name) / 'drive'
        common = ['g++', '-std=c++17', '-O2', '-pthread', '-DHOST_TEST', '-I' + str(ROOT / 'include')]
        subprocess.run([*common, '-I' + str(ROOT / 'tests/stubs'),
                        *[str(ROOT / p) for p in ('tests/host/ftp.cpp', 'src/network/wifi_transfer.cpp', 'src/network/socket_runtime.cpp',
                                                  'src/diagnostics/logger.cpp')], '-o', str(cls.ftp_binary)],
                       check=True, capture_output=True)
        subprocess.run([*common, '-I' + str(ROOT / 'tests/stubs'), *[str(ROOT / p) for p in ('tests/host/drive.cpp', 'src/cloud/gdrive.cpp', 'src/cloud/download_manager.cpp', 'src/security/oauth.cpp', 'src/network/socket_runtime.cpp', 'src/cloud/credential_store.cpp', 'src/diagnostics/logger.cpp', 'tests/host/fsdev_rename.cpp')],
                        '-Wl,--wrap=rename', '-o', str(cls.drive_binary)], check=True, capture_output=True)

    @classmethod
    def tearDownClass(cls):
        cls.build.cleanup()

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='sfm-network-data-')
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        (self.root / 'sdmc:').mkdir()

    @contextlib.contextmanager
    def server_process(self, command, expose_process=False):
        p = subprocess.Popen(list(map(str, command)), cwd=self.root, stdin=subprocess.PIPE,
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            ready, _, _ = select.select([p.stdout], [], [], 10)
            self.assertTrue(ready, 'Servidor não anunciou a URL em 10 segundos')
            url = p.stdout.readline().strip()
            self.assertTrue(url, 'Servidor encerrou antes de anunciar URL')
            yield (url, p) if expose_process else url
        finally:
            try:
                out, err = p.communicate('stop\n', timeout=10)
            except subprocess.TimeoutExpired:
                p.kill(); p.communicate(); self.fail('Servidor não parou em 10 segundos')
            self.assertEqual(p.returncode, 0, err)

    def ftp_connect(self, url):
        target = urlparse(url)
        ftp = ftplib.FTP()
        ftp.connect(target.hostname, target.port, timeout=5)
        ftp.login()
        self.addCleanup(ftp.close)
        return ftp

    def test_ftp_passive_roundtrip_and_rename(self):
        with self.server_process([self.ftp_binary]) as url:
            ftp = self.ftp_connect(url)
            ftp.mkd('pasta')
            ftp.cwd('pasta')
            data = bytes(range(256)) * 5000
            ftp.storbinary('STOR arquivo.bin', io.BytesIO(data))
            got = io.BytesIO()
            ftp.retrbinary('RETR arquivo.bin', got.write)
            self.assertEqual(got.getvalue(), data)
            self.assertEqual(ftp.size('arquivo.bin'), len(data))
            self.assertIn('arquivo.bin', ftp.nlst())
            ftp.rename('arquivo.bin', 'renomeado.bin')
            self.assertTrue((self.root / 'sdmc:/pasta/renomeado.bin').exists())
            ftp.delete('renomeado.bin')
            ftp.cwd('/'); ftp.rmd('pasta'); ftp.quit()

    def test_ftp_active_mode_resume_and_preserve_part(self):
        with self.server_process([self.ftp_binary]) as url:
            ftp = self.ftp_connect(url)
            ftp.set_pasv(False)
            data = b'abcdefgh' * 50000
            ftp.storbinary('STOR arquivo.bin', io.BytesIO(data))
            got = io.BytesIO()
            ftp.retrbinary('RETR arquivo.bin', got.write, rest=1234)
            self.assertEqual(got.getvalue(), data[1234:])
            part = self.root / 'sdmc:/arquivo.bin.sfm-part'
            part.write_bytes(b'preservar')
            with self.assertRaises(ftplib.error_perm):
                ftp.storbinary('STOR arquivo.bin', io.BytesIO(b'novo'))
            self.assertEqual(part.read_bytes(), b'preservar')
            self.assertEqual((self.root / 'sdmc:/arquivo.bin').read_bytes(), data)
            ftp.quit()

    def test_ftp_paths_and_active_target_rejected(self):
        with self.server_process([self.ftp_binary]) as url:
            ftp = self.ftp_connect(url)
            ftp.cwd('../../../../')
            self.assertEqual(ftp.pwd(), '/')
            for command in ('CWD sdmc:/outside', 'CWD a\\b', 'PORT 127,0,0,2,20,20',
                            'EPRT |1|127.0.0.1|21|', 'REST abc'):
                with self.assertRaises(ftplib.error_perm):
                    ftp.sendcmd(command)
            ftp.quit()

    @contextlib.contextmanager
    def mock_drive(self, mode='normal'):
        state = {'mode': mode, 'token_requests': 0, 'requests': [], 'unauthorized': False, 'data': b'dados-drive' * 20000}
        class Handler(BaseHTTPRequestHandler):
            protocol_version = 'HTTP/1.1'
            def log_message(self, *args):
                pass
            def respond(self, code, body, headers=None):
                if isinstance(body, dict): body = json.dumps(body).encode()
                self.send_response(code)
                self.send_header('Content-Length', str(len(body)))
                self.send_header('Connection', 'close')
                for key, value in (headers or {}).items(): self.send_header(key, value)
                self.end_headers()
                self.wfile.write(body)
            def do_POST(self):
                body = self.rfile.read(int(self.headers.get('Content-Length', 0)))
                state['requests'].append(self.path)
                if self.path == '/token':
                    state['token_requests'] += 1
                    state['form'] = parse_qs(body.decode())
                    if mode == 'accounts':
                        form = state['form']
                        if form.get('code') == ['denied-account-B-code']:
                            self.respond(400, {'error': 'invalid_grant'}); return
                        account = 'B' if ('account-B' in form.get('code', [''])[0] or form.get('refresh_token', [''])[0] == 'refresh-B') else 'A'
                        self.respond(200, {'access_token': 'access-' + account, 'refresh_token': 'refresh-' + account, 'expires_in': 3600})
                    elif mode == 'auth-error': self.respond(400, {'error': 'invalid_client'})
                    elif mode == 'invalid-grant': self.respond(400, {'error': 'invalid_grant'})
                    elif mode == 'server-error': self.respond(503, {'error': 'server_error', 'error_description': 'private-fixture-value-never-echo'})
                    elif mode == 'no-refresh': self.respond(200, {'access_token': 'fake-access', 'expires_in': 3600})
                    elif mode == 'invalid-fields': self.respond(200, {'access_token': 12345, 'refresh_token': 67890})
                    elif mode == 'rotate': self.respond(200, {'access_token': 'fake-access', 'refresh_token': 'fake-rotated-refresh', 'expires_in': 3600})
                    else: self.respond(200, {'access_token': 'fake-access', 'refresh_token': 'fake-refresh', 'expires_in': 3600})
                elif self.path.startswith('/upload/'):
                    self.respond(200, {}, {'Location': 'https://www.googleapis.com/session/test'})
                else: self.respond(200, {'id': 'uploaded-empty'})
            def do_GET(self):
                state['requests'].append(self.path)
                if self.path.startswith('/drive/v3/about'):
                    account = 'B' if self.headers.get('Authorization') == 'Bearer access-B' else 'A'
                    if mode == 'accounts': self.respond(200, {'user': {'displayName': 'Conta fictícia ' + account, 'permissionId': 'permission-' + account}})
                    else: self.respond(503, {})  # identificação opcional indisponível
                    return
                if mode == 'accounts': state.setdefault('api_accounts', []).append(self.headers.get('Authorization'))
                if self.path.startswith('/drive/v3/files/test-file?') and 'alt=media' not in self.path:
                    self.respond(200, {'id': 'test-file', 'size': str(len(state['data'])), 'version': str(state.get('version', 1))})
                    return
                if 'alt=media' in self.path:
                    state.setdefault('ranges', []).append(self.headers.get('Range'))
                    state['media_requests'] = state.get('media_requests', 0) + 1
                    if mode in ('cut-range', 'http-change', 'ignore-range', 'media-refresh'):
                        if mode == 'media-refresh' and state['media_requests'] == 1:
                            self.respond(401, b''); return
                        match = re.fullmatch(r'bytes=(\d+)-(\d+)', self.headers.get('Range', ''))
                        start, end = map(int, match.groups())
                        data = state['data']; end = min(end, len(data)-1)
                        payload = data[start:end+1]
                        if mode == 'ignore-range' and state['media_requests'] > 1:
                            self.respond(200, data); return
                        self.send_response(206)
                        self.send_header('Content-Length', str(len(payload)))
                        self.send_header('Content-Range', f'bytes {start}-{end}/{len(data)}')
                        self.send_header('ETag', '"version-' + str(state.get('version', 1)) + '"')
                        self.send_header('Connection', 'close'); self.end_headers()
                        if state['media_requests'] == 1 and mode in ('cut-range', 'ignore-range'):
                            self.wfile.write(payload[:17000]); self.wfile.flush()
                            self.close_connection = True
                        else: self.wfile.write(payload)
                        if mode == 'http-change' and state['media_requests'] == 1:
                            state['version'] = 2; state['data'] = b'X' * len(data)
                        return
                    if mode == 'range-error': self.respond(416, b'')
                    elif mode == 'short-body': self.respond(206, b'curto')
                    else: self.respond(206, state['data'], {'Content-Range': f'bytes 0-{len(state["data"])-1}/{len(state["data"])}'})
                    return
                if mode == 'retry-auth' and not state['unauthorized']:
                    state['unauthorized'] = True
                    self.respond(401, {'error': {'message': 'Token expirado de teste'}})
                elif mode == 'api-error': self.respond(403, {'error': {'message': 'Acesso negado de teste'}})
                elif 'pageToken=' in self.path:
                    self.respond(200, {'files': [{'id': 'file', 'name': 'arquivo.bin', 'mimeType': 'application/octet-stream', 'size': '12'}]})
                else: self.respond(200, {'nextPageToken': 'pagina-2', 'files': [{'id': 'folder', 'name': 'Pasta', 'mimeType': 'application/vnd.google-apps.folder'}]})
            def do_PUT(self):
                state['upload'] = self.rfile.read(int(self.headers.get('Content-Length', 0)))
                self.respond(200, {'id': 'uploaded'})
        server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        worker = threading.Thread(target=server.serve_forever, daemon=True); worker.start()
        try:
            yield server.server_port, state
        finally:
            server.shutdown(); server.server_close(); worker.join()

    def config(self):
        config = self.root / 'config-legada'
        config.mkdir(exist_ok=True)
        (config / 'google.json').write_text(json.dumps({'installed': {'client_id': 'fake-id', 'client_secret': 'fake-secret'}}))
        (config / 'token.txt').write_text('fake-refresh')
        return config

    def drive(self, port, config, action, *args, success=True, extra=()):
        result = subprocess.run([str(self.drive_binary), str(port), str(config), action, *map(str, args), *extra],
                                cwd=self.root, capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode == 0, success, result.stderr)
        return result

    def test_drive_legacy_login_and_pagination(self):
        config = self.config()
        original = (config / 'google.json').read_bytes()
        with self.mock_drive() as (port, state):
            result = self.drive(port, config, 'list')
        self.assertEqual(result.stdout.splitlines(), ['folder|Pasta|0', 'file|arquivo.bin|12'])
        self.assertEqual(state['token_requests'], 1)
        self.assertEqual(state['form']['refresh_token'], ['fake-refresh'])
        self.assertEqual((config / 'google.json').read_bytes(), original)
        self.assertEqual((config / 'token.txt').read_text(), 'fake-refresh')

    def test_drive_download_and_resumable_upload(self):
        config = self.config(); download = self.root / 'download.bin'
        with self.mock_drive() as (port, state):
            self.drive(port, config, 'download', download, len(state['data']))
            self.assertEqual(download.read_bytes(), state['data'])
            self.drive(port, config, 'upload', download)
            self.assertEqual(state['upload'], download.read_bytes())

    def test_drive_api_error(self):
        with self.mock_drive('api-error') as (port, state):
            result = self.drive(port, self.config(), 'list', success=False)
        self.assertIn('Acesso negado de teste', result.stderr)

    def test_drive_rejects_short_download(self):
        config = self.config(); dest = self.root / 'short.bin'
        with self.mock_drive('short-body') as (port, state):
            self.drive(port, config, 'download', dest, len(state['data']), success=False)
        self.assertFalse(dest.exists())

    def test_drive_rejects_unsatisfied_range(self):
        config = self.config(); dest = self.root / 'range.bin'
        with self.mock_drive('range-error') as (port, state):
            self.drive(port, config, 'download', dest, len(state['data']), success=False)
        self.assertFalse(dest.exists())

    def test_drive_preserves_existing_destination(self):
        config = self.config(); dest = self.root / 'existente.bin'
        dest.write_bytes(b'preservar usuario')
        with self.mock_drive() as (port, state):
            self.drive(port, config, 'download', dest, len(state['data']), success=False)
        self.assertEqual(dest.read_bytes(), b'preservar usuario')

    def test_login_server_credentials_roundtrip(self):
        config = self.root / 'login'
        config.mkdir()
        with socket.socket() as s:
            s.bind(('127.0.0.1', 0)); login_port = s.getsockname()[1]
        with self.mock_drive() as (port, state):
            with self.server_process([self.drive_binary, port, config, 'serve', login_port]) as url:
                fake = json.dumps({'installed': {'client_id': 'fake-id', 'client_secret': 'fake-secret'}})
                status, _ = self.post_form(url, '/cred', {'j': fake})
                self.assertEqual(status, 200)
                status, _ = self.login_request(url)
                self.assertEqual(status, 200)
        self.assertEqual(json.loads((config / 'google.json').read_text()), json.loads(fake))
        self.assertEqual(self.account_record(config)['refresh'], 'fake-refresh')
        self.assertFalse((config / 'token.txt').exists())

    def test_login_reports_credential_storage_failure(self):
        config = self.root / 'login-storage-error'
        config.mkdir()
        (config / 'google.json').mkdir()
        (config / 'google.json/preservar.txt').write_text('dados existentes')
        with socket.socket() as s:
            s.bind(('127.0.0.1', 0)); login_port = s.getsockname()[1]
        with self.server_process([self.drive_binary, 0, config, 'serve', login_port]) as url:
            fake = json.dumps({'installed': {'client_id': 'fake-id', 'client_secret': 'fake-secret'}})
            status, _ = self.post_form(url, '/cred', {'j': fake})
            self.assertEqual(status, 400)
        self.assertEqual((config / 'google.json/preservar.txt').read_text(), 'dados existentes')
        self.assertFalse(list(config.glob('*.tmp.*')))

    def test_drive_refreshes_after_unauthorized_without_mixing_responses(self):
        with self.mock_drive('retry-auth') as (port, state):
            result = self.drive(port, self.config(), 'list')
        self.assertEqual(state['token_requests'], 2)
        self.assertIn('folder|Pasta|0', result.stdout)

    def auth_page(self, url):
        target = urlparse(url)
        connection = http.client.HTTPConnection(target.hostname, target.port, timeout=5)
        connection.request('GET', '/')
        response = connection.getresponse(); body = response.read().decode(); connection.close()
        nonce = re.search(r'name="csrf" value="([^"]+)"', body)
        self.assertIsNotNone(nonce, body)
        return nonce[1], body

    def post_form(self, url, route, fields):
        target = urlparse(url)
        nonce, _ = self.auth_page(url)
        fields = dict(fields, csrf=nonce)
        connection = http.client.HTTPConnection(target.hostname, target.port, timeout=5)
        connection.request('POST', route, urlencode(fields), {'Content-Type': 'application/x-www-form-urlencoded'})
        response = connection.getresponse(); status, body = response.status, response.read().decode(); connection.close()
        return status, body

    def login_request(self, url, route='/code', code='fake-code-for-test', returned_state=None):
        nonce, _ = self.auth_page(url)
        return self.post_form(url, route, {'u': 'http://localhost/?' + urlencode({'code': code, 'state': nonce if returned_state is None else returned_state})})

    def account_record(self, config):
        return json.loads((config / 'drive-account.json').read_text())

    def status(self, process):
        process.stdin.write('status\n'); process.stdin.flush()
        ready, _, _ = select.select([process.stdout], [], [], 5)
        self.assertTrue(ready, 'Sem resposta de estado do processo de teste')
        return process.stdout.readline().strip()

    def login_port(self):
        with socket.socket() as listener:
            listener.bind(('127.0.0.1', 0))
            return listener.getsockname()[1]

    def test_drive_rotated_refresh_replaces_legacy_under_fsdev_semantics(self):
        config = self.config()
        with self.mock_drive('rotate') as (port, state):
            self.drive(port, config, 'list')
        self.assertEqual(self.account_record(config)['refresh'], 'fake-rotated-refresh')
        self.assertEqual((config / 'token.txt').read_text(), 'fake-refresh')

    def test_transient_and_invalid_grant_preserve_saved_token_and_credential(self):
        config = self.config()
        credential = (config / 'google.json').read_bytes()
        for mode in ('invalid-grant', 'server-error', 'auth-error'):
            with self.subTest(mode=mode), self.mock_drive(mode) as (port, state):
                result = self.drive(port, config, 'list', success=False)
                self.assertIn('logged=0', result.stdout)
                self.assertEqual((config / 'token.txt').read_text(), 'fake-refresh')
                self.assertEqual((config / 'google.json').read_bytes(), credential)
                self.assertNotIn('private-fixture-value-never-echo', result.stderr)

    def test_refresh_can_reuse_saved_token_when_google_omits_new_refresh(self):
        config = self.config()
        with self.mock_drive('no-refresh') as (port, state):
            self.drive(port, config, 'list')
        self.assertEqual((config / 'token.txt').read_text(), 'fake-refresh')
        self.assertFalse(list(config.glob('token.txt.sfm-*')))

    def test_sd_error_is_not_reported_as_google_refusal_and_can_retry_locally(self):
        config = self.config()
        with self.mock_drive('rotate') as (port, state):
            command = [self.drive_binary, port, config, 'serve', self.login_port(),
                       f'--fault=sincronizar-temporario:{errno.EIO}']
            with self.server_process(command, expose_process=True) as (url, process):
                status, body = self.login_request(url)
                self.assertEqual(status, 400)
                self.assertIn('SD:', body)
                self.assertIn('etapa=sincronizar-temporario', body)
                self.assertIn('errno=' + str(errno.EIO), body)
                self.assertNotIn('O Google recusou', body)
                self.assertIn('Tentar salvar login novamente', body)
                self.assertEqual(self.status(process), 'logged=0 source=1')
                self.assertEqual((config / 'token.txt').read_text(), 'fake-refresh')
                status, body = self.login_request(url, '/retry-save')
                self.assertEqual(status, 200)
                self.assertIn('gravado e conferido', body)
                self.assertEqual(self.status(process), 'logged=1 source=0')
                self.assertEqual(state['token_requests'], 1)
        self.assertEqual(self.account_record(config)['refresh'], 'fake-rotated-refresh')

    def test_final_read_failure_cannot_confirm_login(self):
        config = self.config()
        with self.mock_drive('rotate') as (port, state):
            command = [self.drive_binary, port, config, 'serve', self.login_port(), f'--fault=ler-final:{errno.EIO}']
            with self.server_process(command, expose_process=True) as (url, process):
                status, body = self.login_request(url)
                self.assertEqual(status, 400)
                self.assertIn('etapa=ler-final', body)
                self.assertEqual(self.status(process), 'logged=0 source=1')
                self.assertEqual((config / 'token.txt').read_text(), 'fake-refresh')

    def test_second_read_before_confirming_session_is_required(self):
        config = self.config()
        with self.mock_drive('rotate') as (port, state):
            command = [self.drive_binary, port, config, 'serve', self.login_port(),
                       f'--fault=abrir-leitura-confirmacao:{errno.EIO}']
            with self.server_process(command, expose_process=True) as (url, process):
                status, body = self.login_request(url)
                self.assertEqual(status, 400)
                self.assertIn('etapa=abrir-leitura-confirmacao', body)
                self.assertEqual(self.status(process), 'logged=0 source=1')
                self.assertEqual((config / 'token.txt').read_text(), 'fake-refresh')
                status, body = self.login_request(url, '/retry-save')
                self.assertEqual(status, 200)
                self.assertEqual(self.status(process), 'logged=1 source=0')
                self.assertEqual(state['token_requests'], 1)

    def test_new_authorization_without_refresh_cannot_use_another_saved_account(self):
        config = self.config()
        with self.mock_drive('no-refresh') as (port, state):
            with self.server_process([self.drive_binary, port, config, 'serve', self.login_port()], expose_process=True) as (url, process):
                status, body = self.login_request(url)
                self.assertEqual(status, 400)
                self.assertIn('Google:', body)
                self.assertEqual(self.status(process), 'logged=0 source=4')
        self.assertEqual((config / 'token.txt').read_text(), 'fake-refresh')

    def test_logout_blocks_automatic_login_without_deleting_saved_files(self):
        config = self.config()
        credential = (config / 'google.json').read_bytes()
        with self.mock_drive() as (port, state):
            self.drive(port, config, 'logout')
            self.assertEqual((config / 'token.txt').read_text(), 'fake-refresh')
            self.assertEqual((config / 'google.json').read_bytes(), credential)
            self.assertFalse(self.account_record(config)['enabled'])
            self.drive(port, config, 'list', success=False)
            self.assertEqual(state['token_requests'], 0)
            with self.server_process([self.drive_binary, port, config, 'serve', self.login_port()], expose_process=True) as (url, process):
                status, body = self.login_request(url)
                self.assertEqual(status, 200)
                self.assertEqual(self.status(process), 'logged=1 source=0')
            self.assertTrue(self.account_record(config)['enabled'])

    def test_token_storage_diagnostics_never_contain_oauth_values(self):
        config = self.config()
        with self.mock_drive('rotate') as (port, state):
            with self.server_process([self.drive_binary, port, config, 'serve', self.login_port(),
                                      f'--fault=instalar-novo:{errno.EIO}']) as url:
                status, body = self.login_request(url)
                self.assertEqual(status, 400)
        logs = (self.root / 'sdmc:/switch/Switch File Manager/logs/sfm.log').read_text()
        self.assertIn('etapa=instalar-novo', logs)
        self.assertIn('errno=' + str(errno.EIO), logs)
        for secret in ('fake-secret', 'fake-refresh', 'fake-rotated-refresh', 'fake-access', 'fake-code-for-test'):
            self.assertNotIn(secret, logs)
            self.assertNotIn(secret, body)

    def test_first_login_storage_failure_can_retry_without_new_authorization(self):
        config = self.config()
        (config / 'token.txt').unlink()  # somente fixture, nunca SD do proprietário
        with self.mock_drive() as (port, state):
            command = [self.drive_binary, port, config, 'serve', self.login_port(),
                       f'--fault=sincronizar-temporario:{errno.ENOSPC}']
            with self.server_process(command, expose_process=True) as (url, process):
                status, body = self.login_request(url)
                self.assertEqual(status, 400)
                self.assertIn('SD:', body)
                self.assertEqual(self.status(process), 'logged=0 source=1')
                self.assertFalse((config / 'token.txt').exists())
                status, body = self.login_request(url, '/retry-save')
                self.assertEqual(status, 200)
                self.assertEqual(self.status(process), 'logged=1 source=0')
                self.assertEqual(state['token_requests'], 1)
        self.assertEqual(self.account_record(config)['refresh'], 'fake-refresh')

    def test_missing_readback_is_still_a_local_storage_error(self):
        config = self.config()
        with self.mock_drive('rotate') as (port, state):
            command = [self.drive_binary, port, config, 'serve', self.login_port(),
                       f'--fault=abrir-leitura-confirmacao:{errno.ENOENT}']
            with self.server_process(command, expose_process=True) as (url, process):
                status, body = self.login_request(url)
                self.assertEqual(status, 400)
                self.assertIn('SD:', body)
                self.assertIn('errno=' + str(errno.ENOENT), body)
                self.assertEqual(self.status(process), 'logged=0 source=1')

    def test_invalid_oauth_field_types_cannot_confirm_login(self):
        config = self.config()
        with self.mock_drive('invalid-fields') as (port, state):
            with self.server_process([self.drive_binary, port, config, 'serve', self.login_port()], expose_process=True) as (url, process):
                status, body = self.login_request(url)
                self.assertEqual(status, 400)
                self.assertIn('campos inválidos', body)
                self.assertEqual(self.status(process), 'logged=0 source=4')
        self.assertEqual((config / 'token.txt').read_text(), 'fake-refresh')

    def test_corrupted_local_files_are_preserved_and_diagnosed(self):
        config = self.config()
        (config / 'token.txt').write_bytes(b'  \n')
        with self.mock_drive() as (port, state):
            result = self.drive(port, config, 'list', success=False)
            self.assertIn('etapa=validar-token-salvo', result.stderr)
            self.assertEqual(state['token_requests'], 0)
        self.assertEqual((config / 'token.txt').read_bytes(), b'  \n')
        (config / 'google.json').write_text('{json-invalido')
        with self.mock_drive() as (port, state):
            result = self.drive(port, config, 'list', success=False)
            self.assertIn('etapa=validar-credencial', result.stderr)
            self.assertEqual(state['token_requests'], 0)
        self.assertEqual((config / 'google.json').read_text(), '{json-invalido')

    def command(self, process, action):
        process.stdin.write(action + '\n'); process.stdin.flush()
        ready, _, _ = select.select([process.stdout], [], [], 5)
        self.assertTrue(ready, action)
        return process.stdout.readline().strip()

    def test_pkce_and_state_are_bound_to_current_authorization(self):
        config = self.config()
        with self.mock_drive() as (port, state):
            with self.server_process([self.drive_binary, port, config, 'serve', self.login_port()]) as url:
                nonce, page = self.auth_page(url)
                auth = html.unescape(re.search(r'href="(https://accounts.google.com/[^"]+)"', page)[1])
                query = parse_qs(urlparse(auth).query)
                self.assertEqual(query['state'], [nonce])
                self.assertEqual(query['code_challenge_method'], ['S256'])
                self.assertIn('select_account', query['prompt'][0])
                status, _ = self.login_request(url, returned_state='wrong-state')
                self.assertEqual(status, 400); self.assertEqual(state['token_requests'], 0)
                status, _ = self.login_request(url)
                self.assertEqual(status, 200)
                verifier = state['form']['code_verifier'][0]
                digest = base64.urlsafe_b64encode(hashlib.sha256(verifier.encode()).digest()).decode().rstrip('=')
                self.assertEqual(query['code_challenge'], [digest])

    def test_local_post_requires_csrf_nonce(self):
        config = self.config()
        with self.mock_drive() as (port, state):
            with self.server_process([self.drive_binary, port, config, 'serve', self.login_port()]) as url:
                target = urlparse(url)
                connection = http.client.HTTPConnection(target.hostname, target.port, timeout=5)
                connection.request('POST', '/code', urlencode({'u': 'fake-code', 'csrf': 'incorrect'}))
                response = connection.getresponse(); response.read(); connection.close()
                self.assertEqual(response.status, 400); self.assertEqual(state['token_requests'], 0)

    def test_keep_connected_disabled_blocks_auto_restore_but_allows_explicit_reconnect(self):
        config = self.config()
        with self.mock_drive() as (port, state):
            self.drive(port, config, 'keep-off')
            self.assertFalse(self.account_record(config)['keep'])
            self.drive(port, config, 'list', success=False)
            self.assertEqual(state['token_requests'], 0)
            self.drive(port, config, 'reconnect')
            self.assertFalse(self.account_record(config)['keep'])
            self.drive(port, config, 'list', success=False)
            self.assertEqual(state['token_requests'], 1)
        self.assertEqual((config / 'token.txt').read_text(), 'fake-refresh')

    def test_keep_connected_enabled_restores_on_next_process(self):
        config = self.config()
        with self.mock_drive() as (port, state):
            self.drive(port, config, 'keep-off'); self.drive(port, config, 'keep-on')
            self.drive(port, config, 'list')
            self.drive(port, config, 'list')
            self.assertEqual(state['token_requests'], 2)
        self.assertTrue(self.account_record(config)['keep'])

    def test_logout_can_reconnect_without_new_authorization(self):
        config = self.config()
        with self.mock_drive('accounts') as (port, state):
            self.drive(port, config, 'list'); self.drive(port, config, 'logout')
            self.drive(port, config, 'list', success=False)
            self.drive(port, config, 'reconnect')
            self.assertEqual(state['form']['grant_type'], ['refresh_token'])
            self.assertTrue(self.account_record(config)['enabled'])

    def test_account_switch_commits_new_account_without_mixing_tokens(self):
        config = self.config()
        with self.mock_drive('accounts') as (port, state):
            self.drive(port, config, 'list')
            previous = self.account_record(config)
            with self.server_process([self.drive_binary, port, config, 'serve', self.login_port(), '--restore'], expose_process=True) as (url, process):
                self.assertIn('logged=0', self.status(process))  # operações antigas pausadas
                self.assertEqual(self.command(process, 'list'), 'list=0')
                status, _ = self.login_request(url, code='account-B-code')
                self.assertEqual(status, 200)
                self.assertEqual(self.status(process), 'logged=1 source=0')
                current = self.account_record(config)
                self.assertEqual(current['refresh'], 'refresh-B')
                self.assertNotEqual(current['key'], previous['key'])
                self.assertEqual(current['name'], 'Conta fictícia B')
                self.assertEqual(self.command(process, 'list'), 'list=1')
                self.assertEqual(state['api_accounts'][-1], 'Bearer access-B')
            self.drive(port, config, 'list')
            self.assertEqual(state['form']['refresh_token'], ['refresh-B'])
            self.assertEqual((config / 'token.txt').read_text(), 'fake-refresh')

    def test_cancelled_switch_restores_previous_session_and_files(self):
        config = self.config()
        with self.mock_drive('accounts') as (port, state):
            self.drive(port, config, 'list')
            with self.server_process([self.drive_binary, port, config, 'serve', self.login_port(), '--restore'], expose_process=True) as (url, process):
                before = (config / 'drive-account.json').read_bytes()
                self.assertEqual(self.command(process, 'cancel'), 'cancelled')
                self.assertEqual(self.status(process), 'logged=1 source=0')
                self.assertEqual(self.command(process, 'list'), 'list=1')
                self.assertEqual(state['api_accounts'][-1], 'Bearer access-A')
                self.assertEqual((config / 'drive-account.json').read_bytes(), before)

    def test_failed_switch_storage_preserves_previous_account(self):
        config = self.config()
        with self.mock_drive('accounts') as (port, state):
            self.drive(port, config, 'list')
            with self.server_process([self.drive_binary, port, config, 'serve', self.login_port(), '--restore',
                                      f'--fault=sincronizar-temporario:{errno.ENOSPC}'], expose_process=True) as (url, process):
                status, body = self.login_request(url, code='account-B-code')
                self.assertEqual(status, 400); self.assertIn('SD:', body)
                self.assertEqual(self.account_record(config)['refresh'], 'refresh-A')
                self.assertEqual(self.command(process, 'cancel'), 'cancelled')
                self.assertEqual(self.status(process), 'logged=1 source=0')
                self.assertEqual(self.command(process, 'list'), 'list=1')
                self.assertEqual(state['api_accounts'][-1], 'Bearer access-A')

    def test_failed_switch_confirmation_restores_persistent_previous_account(self):
        config = self.config()
        with self.mock_drive('accounts') as (port, state):
            self.drive(port, config, 'list')
            # skip=1: a confirmação da restauração --restore ocorre antes da nova autorização.
            with self.server_process([self.drive_binary, port, config, 'serve', self.login_port(), '--restore',
                                      f'--fault=abrir-leitura-confirmacao:{errno.EIO}:1'], expose_process=True) as (url, process):
                status, _ = self.login_request(url, code='account-B-code')
                self.assertEqual(status, 400)
                self.assertEqual(self.account_record(config)['refresh'], 'refresh-A')
                self.command(process, 'cancel')
                self.assertIn('logged=1', self.status(process))

    def test_forget_adopted_legacy_account_cleans_only_its_authentication(self):
        config = self.config()
        (config / 'favoritos.txt').write_text('favorito de teste')
        (config / 'historico.txt').write_text('histórico de teste')
        (config / 'token.txt.tmp.OLD').write_text('fake-refresh')
        credential = (config / 'google.json').read_bytes()
        with self.mock_drive('accounts') as (port, state):
            self.drive(port, config, 'list')
            self.drive(port, config, 'forget')
            record = self.account_record(config)
            self.assertTrue(record['forgotten']); self.assertEqual(record['refresh'], '')
            self.assertFalse((config / 'token.txt').exists())
            self.assertFalse((config / 'token.txt.tmp.OLD').exists())
            for path in config.glob('drive-account.json*'):
                text = path.read_text()
                self.assertNotIn('refresh-A', text); self.assertNotIn('fake-refresh', text)
            requests = state['token_requests']
            self.drive(port, config, 'list', success=False)
            self.drive(port, config, 'reconnect', success=False)
            self.assertEqual(state['token_requests'], requests)
        self.assertEqual((config / 'google.json').read_bytes(), credential)
        self.assertEqual((config / 'favoritos.txt').read_text(), 'favorito de teste')
        self.assertEqual((config / 'historico.txt').read_text(), 'histórico de teste')

    def test_forget_new_account_preserves_other_account_backups(self):
        config = self.config()
        with self.mock_drive('accounts') as (port, state):
            self.drive(port, config, 'list')
            with self.server_process([self.drive_binary, port, config, 'serve', self.login_port()]) as url:
                self.assertEqual(self.login_request(url, code='account-B-code')[0], 200)
            self.drive(port, config, 'forget')
        texts = [p.read_text() for p in config.glob('drive-account.json*')]
        self.assertTrue(any('refresh-A' in text for text in texts))
        self.assertFalse(any('refresh-B' in text for text in texts))
        self.assertEqual((config / 'token.txt').read_text(), 'fake-refresh')

    def test_failed_forget_can_resume_without_restoring_login(self):
        config = self.config()
        with self.mock_drive('accounts') as (port, state):
            self.drive(port, config, 'list')
            result = self.drive(port, config, 'forget', success=False,
                                extra=(f'--fault=apagar-autenticacao:{errno.EROFS}',))
            self.assertIn('restaram dados locais', result.stderr)
            self.assertTrue(self.account_record(config)['forgotten'])
            self.drive(port, config, 'list', success=False)
            self.drive(port, config, 'forget')
            self.assertFalse((config / 'token.txt').exists())
            self.assertTrue(self.account_record(config)['forgotten'])

    def test_forget_reports_unattributed_partial_credentials_and_preserves_them(self):
        config = self.config()
        partial = config / 'drive-account.json.sfm-new-050'
        partial.write_text('{"refresh":"fictional-partial')
        with self.mock_drive('accounts') as (port, state):
            self.drive(port, config, 'list')
            result = self.drive(port, config, 'forget', success=False)
            self.assertIn('não atribuíveis', result.stderr)
            self.assertTrue(self.account_record(config)['forgotten'])
        self.assertEqual(partial.read_text(), '{"refresh":"fictional-partial')

    def test_client_binding_prevents_using_tokens_with_different_oauth_client(self):
        config = self.config()
        with self.mock_drive('accounts') as (port, state):
            self.drive(port, config, 'list')
            (config / 'google.json').write_text(json.dumps({'installed': {'client_id': 'another-id', 'client_secret': 'another-fake-secret'}}))
            before = state['token_requests']
            result = self.drive(port, config, 'list', success=False)
            self.assertIn('credencial do aplicativo mudou', result.stderr)
            self.assertEqual(state['token_requests'], before)
            self.assertEqual(self.account_record(config)['refresh'], 'refresh-A')

    def test_account_record_recovers_after_interruption_between_renames(self):
        config = self.config()
        with self.mock_drive() as (port, state):
            self.drive(port, config, 'list')
            account = config / 'drive-account.json'
            account.rename(config / 'drive-account.json.sfm-old-222')
            (config / 'drive-account.json.sfm-new-222').write_text('unconfirmed-fictional-candidate')
            self.drive(port, config, 'list')
            self.assertEqual(self.account_record(config)['refresh'], 'fake-refresh')
            self.assertEqual((config / 'drive-account.json.sfm-new-222').read_text(), 'unconfirmed-fictional-candidate')

    def test_account_errors_and_preferences_do_not_leak_secrets_in_logs(self):
        config = self.config()
        with self.mock_drive('accounts') as (port, state):
            self.drive(port, config, 'list'); self.drive(port, config, 'keep-off'); self.drive(port, config, 'logout')
            self.drive(port, config, 'reconnect'); self.drive(port, config, 'forget')
        logs = (self.root / 'sdmc:/switch/Switch File Manager/logs/sfm.log').read_text()
        for secret in ('refresh-A', 'access-A', 'fake-refresh', 'fake-secret', 'permission-A'):
            self.assertNotIn(secret, logs)

    def test_connection_states_distinguish_expired_network_and_storage_errors(self):
        config = self.config()
        with self.mock_drive('invalid-grant') as (port, state):
            result = self.drive(port, config, 'list', success=False)
            self.assertIn('state=Autorização expirada ou inválida', result.stdout)
        result = self.drive(0, config, 'list', success=False)
        self.assertIn('state=Erro de rede', result.stdout)
        (config / 'drive-account.json').write_text('{corrupted')
        result = self.drive(0, config, 'list', success=False)
        self.assertIn('state=Erro de armazenamento no SD', result.stdout)
        self.assertEqual((config / 'drive-account.json').read_text(), '{corrupted')

    def test_preference_failure_does_not_acknowledge_unsaved_change(self):
        config = self.config()
        result = self.drive(0, config, 'keep-off', success=False,
                            extra=(f'--fault=sincronizar-temporario:{errno.EROFS}',))
        self.assertIn('SD:', result.stderr)
        self.assertFalse((config / 'drive-account.json').exists())
        with self.mock_drive() as (port, state):
            self.drive(port, config, 'list')
            self.assertTrue(self.account_record(config)['keep'])

    def test_forget_before_persistent_block_fails_without_deleting_authentication(self):
        config = self.config()
        with self.mock_drive('accounts') as (port, state):
            self.drive(port, config, 'list')
            before = (config / 'drive-account.json').read_bytes()
            self.drive(port, config, 'forget', success=False,
                       extra=(f'--fault=sincronizar-temporario:{errno.ENOSPC}',))
            self.assertEqual((config / 'drive-account.json').read_bytes(), before)
            self.assertEqual((config / 'token.txt').read_text(), 'fake-refresh')

    def test_forget_resumes_legacy_token_deletion_after_account_snapshots_are_gone(self):
        config = self.config()
        with self.mock_drive('accounts') as (port, state):
            self.drive(port, config, 'list')
            self.drive(port, config, 'forget', success=False,
                       extra=(f'--fault=apagar-autenticacao:{errno.EROFS}:1',))
            self.assertTrue((config / 'token.txt').exists())
            self.assertTrue(self.account_record(config)['forgotten'])
            self.drive(port, config, 'forget')
            self.assertFalse((config / 'token.txt').exists())

    def test_switch_failure_from_google_restores_previous_account_on_cancel(self):
        config = self.config()
        with self.mock_drive('accounts') as (port, state):
            self.drive(port, config, 'list')
            with self.server_process([self.drive_binary, port, config, 'serve', self.login_port(), '--restore'], expose_process=True) as (url, process):
                before = (config / 'drive-account.json').read_bytes()
                status, _ = self.login_request(url, code='denied-account-B-code')
                self.assertEqual(status, 400)
                self.assertEqual((config / 'drive-account.json').read_bytes(), before)
                self.assertEqual(self.command(process, 'cancel'), 'cancelled')
                self.assertEqual(self.status(process), 'logged=1 source=0')
                self.assertEqual(self.command(process, 'list'), 'list=1')
                self.assertEqual(state['api_accounts'][-1], 'Bearer access-A')

    def test_http_disconnect_resumes_range_and_publishes_complete_content(self):
        config = self.config(); dest = self.root / 'recovered-http.bin'
        with self.mock_drive('cut-range') as (port, state):
            self.drive(port, config, 'download', dest, len(state['data']))
            self.assertEqual(dest.read_bytes(), state['data'])
            self.assertIn('bytes=17000-', state['ranges'][1])

    def test_http_remote_change_does_not_mix_old_and_new_versions(self):
        config = self.config(); dest = self.root / 'changed-http.bin'
        with self.mock_drive('http-change') as (port, state):
            self.drive(port, config, 'download', dest, len(state['data']))
            self.assertEqual(dest.read_bytes(), b'X' * len(state['data']))
            self.assertGreaterEqual(state['media_requests'], 2)

    def test_http_ignored_range_restarts_from_zero(self):
        config = self.config(); dest = self.root / 'ignored-range.bin'
        with self.mock_drive('ignore-range') as (port, state):
            self.drive(port, config, 'download', dest, len(state['data']))
            self.assertEqual(dest.read_bytes(), state['data'])
            self.assertEqual(state['ranges'][2].split('-')[0], 'bytes=0')

    def test_download_http_401_renews_token_without_new_consent(self):
        config = self.config(); dest = self.root / 'refresh-download.bin'
        with self.mock_drive('media-refresh') as (port, state):
            self.drive(port, config, 'download', dest, len(state['data']))
            self.assertEqual(dest.read_bytes(), state['data'])
            self.assertEqual(state['token_requests'], 2)

    def test_download_protocol_error_preserves_account_connection_state(self):
        config = self.config(); dest = self.root / 'protocol-failure.bin'
        original = (config / 'google.json').read_bytes()
        with self.mock_drive('range-error') as (port, state):
            result = self.drive(port, config, 'download', dest, len(state['data']), success=False)
            self.assertIn('logged=1 state=Conectado', result.stdout)
            self.assertEqual(state['token_requests'], 1)
        self.assertEqual((config / 'google.json').read_bytes(), original)
        self.assertEqual((config / 'token.txt').read_text(), 'fake-refresh')
