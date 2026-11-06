from pathlib import Path
import base64
import hashlib
import ftplib
import io
import json
import os
import queue
import struct
import subprocess
import tempfile
import threading
import time
import unittest
from urllib.parse import urlparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = Path(__file__).resolve().parents[1]
OFFICIAL = "sdmc:/switch/Switch File Manager"
TARGET = OFFICIAL + "/SwitchFileManager.nro"


def nro(ver="1.0.1", helper=False):
    data = bytearray(0x3000)
    data[0x10:0x14] = b"NRO0"
    struct.pack_into("<I", data, 0x18, len(data))
    for offset, start in ((0x20, 0), (0x28, 0x1000), (0x30, 0x2000)):
        struct.pack_into("<II", data, offset, start, 0x1000)
    marker = ("SFM_BUILD:v" + ver + "-aaaaaaaaaaaa\0").encode()
    data[0x100 : 0x100 + len(marker)] = marker
    icon = b"\xff\xd8\xff\xd9"
    aset = bytearray(0x38)
    aset[:4] = b"ASET"
    struct.pack_into("<QQQQ", aset, 8, 0x38, len(icon), 0x38 + len(icon), 0x4000)
    nacp = bytearray(0x4000)
    title = (
        "Atualizador do Switch File Manager" if helper else "Switch File Manager"
    ).encode()
    nacp[: len(title)] = title
    nacp[0x3060 : 0x3060 + len(ver)] = ver.encode()
    return bytes(data + aset + icon + nacp)


def manifest(channel="experimental"):
    base = "https://github.com/TemudjinBR/switch-file-manager/releases/download/v1.0.1/"

    def asset(data, name):
        return {
            "url": base + name,
            "size": len(data),
            "sha256": hashlib.sha256(data).hexdigest(),
        }

    return {
        "schema": 1,
        "min_updater": 1,
        "version": "1.0.1",
        "build": "v1.0.1-aaaaaaaaaaaa",
        "commit": "a" * 40,
        "tag": "v1.0.1",
        "channel": channel,
        "notes": "Notas fictícias 🔒",
        "app": asset(nro(), "SwitchFileManager.nro"),
        "helper": asset(nro(helper=True), "SfmUpdateHelper.nro"),
    }


class UpdaterTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory(prefix="sfm-update-build-")
        cls.binary = Path(cls.build.name) / "updater"
        sources = [
            "tests/host/updater.cpp",
            "src/updates/update_core.cpp",
            "src/updates/update_service.cpp",
            "src/network/update_http.cpp",
            "src/network/wifi_transfer.cpp",
            "src/network/socket_runtime.cpp",
            "src/cloud/credential_store.cpp",
            "src/diagnostics/logger.cpp",
            "tests/host/fsdev_rename.cpp",
        ]
        p = subprocess.run(
            [
                "g++",
                *(
                    ["-fsanitize=thread", "-g", "-fno-omit-frame-pointer"]
                    if os.environ.get("SFM_TEST_TSAN")
                    else []
                ),
                "-std=c++17",
                "-O2",
                "-pthread",
                "-DHOST_TEST",
                "-I" + str(ROOT / "include"),
                "-I" + str(ROOT / "tests/stubs"),
                *[str(ROOT / p) for p in sources],
                "-Wl,--wrap=rename",
                "-o",
                str(cls.binary),
            ],
            capture_output=True,
            text=True,
        )
        if p.returncode:
            raise RuntimeError(p.stderr)

    @classmethod
    def tearDownClass(cls):
        cls.build.cleanup()

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="sfm-updater-")
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.official = self.root / OFFICIAL
        self.official.mkdir(parents=True)
        self.target = self.root / TARGET
        self.old = nro("1.0.0")
        self.target.write_bytes(self.old)
        self.app = self.root / "fixture.nro"
        self.app.write_bytes(nro())
        self.helper = self.root / "helper-fixture.nro"
        self.helper.write_bytes(nro(helper=True))
        self.m = manifest()
        self.mpath = self.root / "manifest.json"
        self.mpath.write_text(json.dumps(self.m))

    def run_tool(self, *args, code=0):
        p = subprocess.run(
            [str(self.binary), *map(str, args)],
            cwd=self.root,
            text=True,
            capture_output=True,
            timeout=15,
        )
        self.assertEqual(p.returncode, code, p.stderr)
        return p

    def prepare(self):
        return self.run_tool("prepare", self.mpath, self.app, self.helper).stdout

    def test_semver_numeric_no_downgrade_or_lexical_comparison(self):
        for a, b, result in [
            ("1.2.9", "1.2.10", -1),
            ("2.0.0", "1.99.99", 1),
            ("1.0.0", "1.0.0", 0),
        ]:
            self.assertEqual(self.run_tool("compare", a, b).stdout, str(result))
        for value in ["1.2", "v1.2.3", "01.2.3", "1.2.3-beta", "1.2.3.4", "1.2.3\n"]:
            self.run_tool("compare", value, "1.0.0", code=2)

    def test_sha256_against_hashlib_for_block_boundaries(self):
        for size in [0, 1, 55, 56, 63, 64, 65, 32769, 200000]:
            data = bytes((i * 17) % 256 for i in range(size))
            self.app.write_bytes(data)
            self.assertEqual(
                self.run_tool("hash", self.app).stdout, hashlib.sha256(data).hexdigest()
            )

    def test_manifest_channel_identity_origin_and_protocol_rejected(self):
        self.run_tool("manifest", self.mpath, "experimental")
        self.run_tool("manifest", self.mpath, "stable", code=1)
        for key, value in [
            ("build", "v1.0.0-aaaaaaaaaaaa"),
            ("tag", "v1.0.2"),
            ("min_updater", 2),
            ("version", "01.0.0"),
        ]:
            changed = dict(self.m)
            changed[key] = value
            self.mpath.write_text(json.dumps(changed))
            self.run_tool("manifest", self.mpath, "experimental", code=1)
        changed = manifest()
        changed["app"]["url"] = "http://github.com/file"
        self.mpath.write_text(json.dumps(changed))
        self.run_tool("manifest", self.mpath, "experimental", code=1)

    def test_duplicate_json_keys_and_bad_unicode_fail_closed(self):
        for text in [
            '{"schema":1,"schema":1}',
            json.dumps(self.m)[:-1] + ',"unknown":"\\ud800"}',
            '{"value":"\\u0000"}',
        ]:
            self.mpath.write_text(text)
            self.run_tool("manifest", self.mpath, "experimental", code=1)

    def test_install_helper_idempotence_and_explicit_rollback(self):
        directory = self.prepare()
        self.assertEqual(self.target.read_bytes(), self.old)
        self.run_tool("install", directory)
        self.assertEqual(self.target.read_bytes(), nro())
        self.assertEqual((self.root / directory / "old.nro").read_bytes(), self.old)
        self.run_tool("install", directory)
        self.run_tool("rollback", directory)
        self.assertEqual(self.target.read_bytes(), self.old)
        self.run_tool("rollback", directory)
        self.assertEqual((self.root / directory / "reverted.nro").read_bytes(), nro())

    def test_power_loss_after_backup_can_resume_without_overwrite(self):
        directory = self.prepare()
        self.run_tool("install", directory, "crash=apos-backup", code=77)
        self.assertFalse(self.target.exists())
        self.run_tool("install", directory)
        self.assertEqual(self.target.read_bytes(), nro())

    def test_power_loss_after_publication_is_idempotent(self):
        directory = self.prepare()
        self.run_tool("install", directory, "crash=apos-publicar", code=77)
        self.run_tool("install", directory)
        self.assertEqual(self.target.read_bytes(), nro())

    def test_install_failure_restores_original_before_return(self):
        for stage in ["apos-backup", "publicar-nro", "espaco-livre"]:
            with self.subTest(stage=stage):
                directory = self.prepare()
                self.run_tool("install", directory, "fail=" + stage, code=1)
                self.assertEqual(self.target.read_bytes(), self.old)

    def test_checksum_mismatch_and_changed_destination_preserved(self):
        self.app.write_bytes(nro()[:-3])
        self.run_tool("prepare", self.mpath, self.app, self.helper, code=1)
        self.assertEqual(self.target.read_bytes(), self.old)
        self.app.write_bytes(nro())
        directory = self.prepare()
        self.target.write_bytes(b"personal fixture")
        self.run_tool("install", directory, code=1)
        self.assertEqual(self.target.read_bytes(), b"personal fixture")

    def test_wrong_running_path_and_stale_build_are_blocked(self):
        self.run_tool("target", TARGET, "1.0.0", "v1.0.0-aaaaaaaaaaaa")
        for path in [
            "indisponivel",
            "sdmc:/switch/duplicate.nro",
            "/switch/Switch File Manager/SwitchFileManager.nro",
        ]:
            self.run_tool("target", path, "1.0.0", "v1.0.0-aaaaaaaaaaaa", code=1)
        self.run_tool("target", TARGET, "1.0.1", "v1.0.1-aaaaaaaaaaaa", code=1)

    def test_unrelated_files_and_preexisting_backup_are_preserved(self):
        keep = self.official / "google.json"
        keep.write_bytes(b"personal fixture")
        directory = self.prepare()
        backup = self.root / directory / "old.nro"
        backup.write_bytes(b"foreign file")
        self.run_tool("install", directory, code=1)
        self.assertEqual(backup.read_bytes(), b"foreign file")
        self.assertEqual(keep.read_bytes(), b"personal fixture")
        self.assertEqual(self.target.read_bytes(), self.old)

    def api_server(self, mode="normal", channel="stable"):
        owner = self
        owner.gate = threading.Event()
        owner.release = threading.Event()
        m = manifest(channel)
        base = "/TemudjinBR/switch-file-manager/releases/download/v1.0.1/"

        class Handler(BaseHTTPRequestHandler):
            protocol_version = "HTTP/1.1"

            def log_message(self, *args):
                pass

            def send(self, code, body, headers=None):
                if isinstance(body, (dict, list)):
                    body = json.dumps(body).encode()
                self.send_response(code)
                self.send_header("Content-Length", str(len(body)))
                self.send_header("Connection", "close")
                for key, value in (headers or {}).items():
                    self.send_header(key, value)
                self.end_headers()
                try:
                    self.wfile.write(body)
                except (BrokenPipeError, ConnectionResetError):
                    pass

            def do_GET(self):
                if self.path.startswith("/repos/"):
                    if mode == "offline":
                        self.send(503, b"")
                        return
                    if mode == "rate":
                        self.send(429, b"", {"Retry-After": "900"})
                        return
                    row = {
                        "tag_name": "v1.0.1",
                        "target_commitish": "a" * 40,
                        "draft": False,
                        "prerelease": channel == "experimental",
                        "assets": [
                            {
                                "name": "update.json",
                                "browser_download_url": "https://github.com"
                                + base
                                + "update.json",
                                "size": len(json.dumps(m)),
                            },
                            {
                                "name": "SwitchFileManager.nro",
                                "browser_download_url": m["app"]["url"],
                                "size": m["app"]["size"],
                            },
                            {
                                "name": "SfmUpdateHelper.nro",
                                "browser_download_url": m["helper"]["url"],
                                "size": m["helper"]["size"],
                            },
                        ],
                    }
                    if mode == "other-channel":
                        row["prerelease"] = not row["prerelease"]
                    self.send(200, [row] if "?" in self.path else row)
                elif self.path == base + "update.json":
                    self.send(200, m)
                elif self.path.endswith("SwitchFileManager.nro"):
                    if mode == "gate":
                        owner.gate.set()
                        owner.release.wait(8)
                    if mode == "redirect":
                        self.send(
                            302, b"", {"Location": "http://untrusted.invalid/payload"}
                        )
                        return
                    if mode == "chunked":
                        self.send_response(200)
                        self.send_header("Transfer-Encoding", "chunked")
                        self.send_header("Connection", "close")
                        self.end_headers()
                        data = nro()
                        self.wfile.write(
                            f"{len(data):x}\r\n".encode() + data + b"\r\n0\r\n\r\n"
                        )
                        return
                    data = nro()
                    if mode == "checksum":
                        data = data[:-1] + b"X"
                    if mode == "partial":
                        self.send_response(200)
                        self.send_header("Content-Length", str(len(data)))
                        self.send_header("Connection", "close")
                        self.end_headers()
                        self.wfile.write(data[:1000])
                        return
                    self.send(200, data)
                else:
                    self.send(200, nro(helper=True))

        server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()

        def close():
            owner.release.set()
            server.shutdown()
            server.server_close()
            thread.join()

        self.addCleanup(close)
        return server.server_port

    def service(self, port):
        process = subprocess.Popen(
            [str(self.binary), "service", str(port)],
            cwd=self.root,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )
        lines = queue.Queue()

        def reader():
            for line in process.stdout:
                lines.put(line.strip())

        threading.Thread(target=reader, daemon=True).start()

        def command(value):
            process.stdin.write(value + "\n")
            process.stdin.flush()
            try:
                return lines.get(timeout=5)
            except queue.Empty:
                self.fail("UI/worker nao respondeu; possivel deadlock")

        def close():
            self.release.set()
            try:
                _, error = process.communicate("quit\n", timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.communicate()
                self.fail("Worker nao encerrou")
            self.assertEqual(process.returncode, 0, error)

        self.addCleanup(close)
        self.assertEqual(lines.get(timeout=5), "ready")
        return command

    def wait_worker(self, command):
        for _ in range(400):
            result = command("status")
            if "busy=0" in result:
                return result
            time.sleep(0.02)
        self.fail("Worker excedeu prazo")

    def test_background_check_defaults_to_stable_and_respects_cooldown(self):
        command = self.service(self.api_server())
        self.assertIn("channel=stable", command("status"))
        command("check")
        self.assertIn("version=1.0.1", self.wait_worker(command))
        self.assertIn("intervalo", command("check"))
        self.assertIn("auto=0", command("auto-off"))

    def test_experimental_must_be_selected_and_filters_release_kind(self):
        command = self.service(self.api_server(channel="experimental"))
        self.assertIn("channel=experimental", command("channel=experimental"))
        command("check")
        self.assertIn("version=1.0.1", self.wait_worker(command))
        command("download")
        self.assertIn("ready=1", self.wait_worker(command))
        self.assertEqual(self.target.read_bytes(), self.old)

    def test_channel_does_not_accept_opposite_release(self):
        command = self.service(self.api_server("other-channel"))
        command("check")
        self.assertNotIn("version=1.0.1", self.wait_worker(command))

    def test_partial_or_bad_checksum_never_replaces_installed_nro(self):
        for mode in ("partial", "checksum", "redirect"):
            with self.subTest(mode=mode):
                # Um ambiente independente por processo/arquivo de preferencias.
                for path in self.official.glob("update-settings.json*"):
                    path.unlink()
                command = self.service(self.api_server(mode))
                command("check")
                self.wait_worker(command)
                command("download")
                self.assertIn("Interrompido", self.wait_worker(command))
                self.assertEqual(self.target.read_bytes(), self.old)
                self.assertFalse(
                    any(self.official.glob(".sfm-update-*/transaction.json"))
                )

    def test_chunked_download_is_verified_and_staged(self):
        command = self.service(self.api_server("chunked"))
        command("check")
        self.wait_worker(command)
        command("download")
        self.assertIn("ready=1", self.wait_worker(command))
        self.assertEqual(self.target.read_bytes(), self.old)

    def test_worker_remains_responsive_and_can_cancel_pending_download(self):
        command = self.service(self.api_server("gate"))
        command("check")
        self.wait_worker(command)
        command("download")
        self.assertTrue(self.gate.wait(5))
        self.assertIn("busy=1", command("status"))
        command("cancel")
        self.assertIn("Cancelado", self.wait_worker(command))
        self.assertEqual(self.target.read_bytes(), self.old)

    def test_network_and_rate_errors_are_reported_without_credentials(self):
        for mode in ("offline", "rate"):
            for path in self.official.glob("update-settings.json*"):
                path.unlink()
            command = self.service(self.api_server(mode))
            command("check")
            result = self.wait_worker(command)
            self.assertIn("HTTP " + ("503" if mode == "offline" else "429"), result)
            self.assertEqual(self.target.read_bytes(), self.old)

    def test_rollback_restart_after_moving_new_nro_preserves_both_versions(self):
        directory = self.prepare()
        self.run_tool("install", directory)
        self.run_tool("rollback", directory, "crash=rollback-publicar", code=77)
        self.assertFalse(self.target.exists())
        self.run_tool("rollback", directory)
        self.assertEqual(self.target.read_bytes(), self.old)
        self.assertEqual((self.root / directory / "reverted.nro").read_bytes(), nro())

    def test_ordinary_rollback_failure_restores_new_and_keeps_old_backup(self):
        directory = self.prepare()
        self.run_tool("install", directory)
        result = self.run_tool("rollback", directory, "fail=rollback-publicar", code=1)
        self.assertIn("nova versao restaurada", result.stderr)
        self.assertEqual(self.target.read_bytes(), nro())
        self.assertEqual((self.root / directory / "old.nro").read_bytes(), self.old)
        self.run_tool("rollback", directory)
        self.assertEqual(self.target.read_bytes(), self.old)

    def test_ftp_transfer_while_updater_download_is_pending(self):
        command = self.service(self.api_server("gate"))
        url = command("ftp-on").split()[0].split("=", 1)[1]
        address = urlparse(url)
        command("check")
        self.wait_worker(command)
        command("download")
        self.assertTrue(self.gate.wait(5))
        data = b"fictional FTP while updater works" * 20000
        with ftplib.FTP() as ftp:
            ftp.connect(address.hostname, address.port, timeout=5)
            ftp.login()
            ftp.storbinary("STOR updater-concurrent.bin", io.BytesIO(data))
            got = io.BytesIO()
            ftp.retrbinary("RETR updater-concurrent.bin", got.write)
            self.assertEqual(got.getvalue(), data)
        self.assertEqual(self.target.read_bytes(), self.old)
        self.release.set()
        self.assertIn("ready=1", self.wait_worker(command))
        with ftplib.FTP() as ftp:
            ftp.connect(address.hostname, address.port, timeout=5)
            ftp.login()
            self.assertEqual(ftp.size("updater-concurrent.bin"), len(data))

    def test_nro_assets_outside_file_are_rejected_even_with_matching_checksum(self):
        data = bytearray(nro())
        exe = struct.unpack_from("<I", data, 0x18)[0]
        struct.pack_into("<Q", data, exe + 8, (1 << 64) - 10)
        self.app.write_bytes(data)
        self.m["app"]["sha256"] = hashlib.sha256(data).hexdigest()
        self.mpath.write_text(json.dumps(self.m))
        self.run_tool("prepare", self.mpath, self.app, self.helper, code=1)
        self.assertEqual(self.target.read_bytes(), self.old)

    def test_low_space_during_crash_recovery_restores_previous_target(self):
        directory = self.prepare()
        self.run_tool("install", directory, "crash=apos-backup", code=77)
        self.assertFalse(self.target.exists())
        result = self.run_tool("install", directory, "fail=espaco-livre", code=1)
        self.assertIn("anterior restaurado", result.stderr)
        self.assertEqual(self.target.read_bytes(), self.old)
        self.run_tool("install", directory)
        self.assertEqual(self.target.read_bytes(), nro())

    def test_helper_with_wrong_title_is_rejected_even_if_its_hash_matches(self):
        self.helper.write_bytes(nro(helper=False))
        self.m["helper"]["sha256"] = hashlib.sha256(
            self.helper.read_bytes()
        ).hexdigest()
        self.m["helper"]["size"] = self.helper.stat().st_size
        self.mpath.write_text(json.dumps(self.m))
        self.run_tool("prepare", self.mpath, self.app, self.helper, code=1)
        self.assertEqual(self.target.read_bytes(), self.old)

    def test_corrupted_transaction_cannot_replace_existing_target(self):
        directory = self.prepare()
        path = self.root / directory / "transaction.json"
        record = json.loads(path.read_text())
        record["manifest"]["app"]["sha256"] = "0" * 64
        path.write_text(json.dumps(record))
        self.run_tool("install", directory, code=1)
        self.assertEqual(self.target.read_bytes(), self.old)
