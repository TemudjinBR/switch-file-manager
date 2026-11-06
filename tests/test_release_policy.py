from pathlib import Path
import copy
import json
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from release_policy import (
    promotion_gate,
    release_decision,
    approval_record,
    asset_names,
)


class ReleasePolicyTests(unittest.TestCase):
    def setUp(self):
        self.sha = "a" * 40
        self.version = "1.2.3"
        self.record = {
            "commit": self.sha,
            "version": self.version,
            "nro_sha256": "b" * 64,
            "helper_sha256": "c" * 64,
            "reviewed": True,
            "physical": True,
            "authorize_merge_release": True,
        }
        self.comment = {
            "user": {"login": "TemudjinBR"},
            "body": "HOMOLOGACAO_SFM_1\n" + json.dumps(self.record),
        }
        self.pr = {
            "state": "open",
            "merged": False,
            "draft": False,
            "head": {
                "sha": self.sha,
                "repo": {"full_name": "TemudjinBR/switch-file-manager"},
            },
            "base": {"ref": "main"},
        }
        self.branch = {
            "protected": True,
            "protection": {
                "required_status_checks": {
                    "enforcement_level": "everyone",
                    "contexts": ["tests", "build"],
                }
            },
        }
        self.runs = [
            {
                "id": 1,
                "head_sha": self.sha,
                "name": "Compilar e testar Switch File Manager",
                "conclusion": "success",
            }
        ]
        self.report = (
            "Resultado: APROVADO\nDispositivo: Nintendo Switch Lite\nAtmosphere: fixture\nFormato SD: fixture\n"
            + "b" * 64
            + "\n"
            + "c" * 64
        )

    def gate(self):
        return promotion_gate(
            self.pr,
            self.branch,
            self.runs,
            self.comment,
            self.report,
            "TemudjinBR",
            self.sha,
            self.version,
        )

    def test_ci_green_alone_is_not_physical_authorization(self):
        self.comment["body"] = "CI verde; pode publicar"
        with self.assertRaises(ValueError):
            self.gate()

    def test_only_exact_owner_commit_version_and_all_approvals_pass(self):
        self.assertEqual(self.gate(), self.record)
        for key, value in [
            ("commit", "d" * 40),
            ("version", "1.2.4"),
            ("reviewed", False),
            ("physical", False),
            ("authorize_merge_release", False),
        ]:
            record = dict(self.record)
            record[key] = value
            self.comment["body"] = "HOMOLOGACAO_SFM_1\n" + json.dumps(record)
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.gate()

    def test_unprotected_main_and_admin_bypass_are_blocked(self):
        self.branch["protected"] = False
        with self.assertRaises(ValueError):
            self.gate()
        self.branch["protected"] = True
        self.branch["protection"]["required_status_checks"][
            "enforcement_level"
        ] = "non_admins"
        with self.assertRaises(ValueError):
            self.gate()

    def test_latest_exact_ci_failure_overrides_older_success(self):
        self.runs.append(dict(self.runs[0], id=2, conclusion="failure"))
        with self.assertRaises(ValueError):
            self.gate()

    def test_fork_draft_changed_head_and_pending_report_are_blocked(self):
        for field, value in [("draft", True), ("state", "closed")]:
            old = self.pr[field]
            self.pr[field] = value
            with self.assertRaises(ValueError):
                self.gate()
            self.pr[field] = old
        original = copy.deepcopy(self.pr)
        self.pr["head"]["sha"] = "d" * 40
        with self.assertRaises(ValueError):
            self.gate()
        self.pr = copy.deepcopy(original)
        self.pr["head"]["repo"]["full_name"] = "fixture/fork"
        with self.assertRaises(ValueError):
            self.gate()
        self.pr = original
        self.report += "\nPENDENTE"
        with self.assertRaises(ValueError):
            self.gate()

    def test_different_comment_author_cannot_approve(self):
        self.comment["user"]["login"] = "fixture-other"
        with self.assertRaises(ValueError):
            self.gate()

    def test_release_rerun_is_noop_only_for_identical_assets_and_channel(self):
        manifest = {"channel": "stable", "commit": self.sha, "tag": "v1.2.3"}
        digests = {name: "b" * 64 for name in asset_names("1.2.3")}
        release = {
            "tag_name": "v1.2.3",
            "target_commitish": self.sha,
            "prerelease": False,
            "draft": False,
            "assets": [
                {"name": name, "digest": "sha256:" + digest}
                for name, digest in digests.items()
            ],
        }
        self.assertEqual(release_decision(None, manifest, digests), "create")
        self.assertEqual(release_decision(release, manifest, digests), "noop")
        release["assets"][0]["digest"] = "sha256:" + "c" * 64
        with self.assertRaises(ValueError):
            release_decision(release, manifest, digests)

    def test_experimental_cannot_be_silently_promoted_or_number_reused(self):
        manifest = {"channel": "stable", "commit": self.sha, "tag": "v1.2.3"}
        release = {
            "tag_name": "v1.2.3",
            "target_commitish": self.sha,
            "prerelease": True,
            "draft": False,
            "assets": [],
        }
        with self.assertRaises(ValueError):
            release_decision(release, manifest, {})
