from pathlib import Path
import hashlib
import json
import os
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import github_distribution as controller
from release_policy import asset_names


class DistributionControllerTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.previous = Path.cwd()
        os.chdir(self.tmp.name)
        self.addCleanup(os.chdir, self.previous)
        self.sha = "a" * 40
        self.version = "1.2.3"
        self.directory = Path("dist")
        self.directory.mkdir()
        self.manifest = {
            "version": self.version,
            "tag": "v" + self.version,
            "channel": "experimental",
            "commit": self.sha,
            "app": {"size": 7, "sha256": hashlib.sha256(b"fixture").hexdigest()},
            "helper": {"size": 7, "sha256": hashlib.sha256(b"fixture").hexdigest()},
        }
        for name in asset_names(self.version):
            (self.directory / name).write_bytes(b"fixture")
        self.save_manifest()

    def save_manifest(self):
        (self.directory / "update.json").write_text(json.dumps(self.manifest))

    def api(self, release=None):
        class API:
            calls = []

            def call(inner, path, method="GET", payload=None, missing=False):
                inner.calls.append((path, method, payload))
                if path.startswith("releases/tags"):
                    return release
                if path.startswith("branches/"):
                    return {"commit": {"sha": self.sha}}
                if path.startswith("git/ref/"):
                    return None
                if path == "git/refs":
                    return {"ref": payload["ref"]}
                if path.startswith("pulls/"):
                    return {"merged": False}
                raise AssertionError(path)

        return API()

    def test_experimental_publish_never_merges_and_sets_prerelease(self):
        api = self.api()
        with patch.object(
            controller, "read_version", return_value=self.version
        ), patch.object(controller.subprocess, "run") as run:
            controller.publish(api, self.sha, "experimental")
            command = run.call_args.args[0]
            self.assertIn("--prerelease", command)
            self.assertIn("--verify-tag", command)
            self.assertNotIn("--clobber", command)
            self.assertFalse(any("merge" in c[0] for c in api.calls))

    def test_repeat_identical_release_is_read_only_noop(self):
        assets = [
            {
                "name": p.name,
                "digest": "sha256:" + hashlib.sha256(p.read_bytes()).hexdigest(),
            }
            for p in self.directory.iterdir()
        ]
        api = self.api(
            {
                "tag_name": "v1.2.3",
                "target_commitish": self.sha,
                "prerelease": True,
                "draft": False,
                "assets": assets,
            }
        )
        with patch.object(
            controller, "read_version", return_value=self.version
        ), patch.object(controller.subprocess, "run") as run:
            controller.publish(api, self.sha, "experimental")
            run.assert_not_called()
        self.assertTrue(all(method == "GET" for _, method, _ in api.calls))

    def test_bad_manifest_or_unexpected_file_blocks_before_any_mutation(self):
        (self.directory / "personal-secret.json").write_text("fictional-never-upload")
        api = self.api()
        with patch.object(
            controller, "read_version", return_value=self.version
        ), patch.object(controller.subprocess, "run") as run:
            with self.assertRaises(ValueError):
                controller.publish(api, self.sha, "experimental")
            run.assert_not_called()
        self.assertEqual(api.calls, [])

    def test_stable_gate_failure_has_no_merge_tag_or_release(self):
        self.manifest["channel"] = "stable"
        self.save_manifest()
        api = self.api()
        with patch.object(
            controller, "read_version", return_value=self.version
        ), patch.object(
            controller, "criteria", side_effect=ValueError("Sem homologacao")
        ), patch.object(
            controller.subprocess, "run"
        ) as run:
            with self.assertRaises(ValueError):
                controller.publish(api, self.sha, "stable", 1, 123)
            run.assert_not_called()
        self.assertTrue(all(method == "GET" for _, method, _ in api.calls))

    def test_network_failure_stops_publication(self):
        api = self.api()
        with patch.object(
            controller, "read_version", return_value=self.version
        ), patch.object(
            api, "call", side_effect=RuntimeError("HTTP 503")
        ), patch.object(
            controller.subprocess, "run"
        ) as run:
            with self.assertRaises(RuntimeError):
                controller.publish(api, self.sha, "experimental")
            run.assert_not_called()
