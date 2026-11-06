#!/usr/bin/env python3
"""Controlador de distribuição. Executado apenas em workflows autorizados, nunca pelo app."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import subprocess
import urllib.request
import urllib.error
from release_policy import (
    REPOSITORY,
    require,
    exact_sha,
    promotion_gate,
    release_decision,
    asset_names,
)
from version import read_version


class GitHub:
    def call(self, path, method="GET", payload=None, missing=False):
        token = os.environ.get("GH_TOKEN", "")
        require(bool(token), "GITHUB_TOKEN do workflow indisponível")
        request = urllib.request.Request(
            "https://api.github.com/repos/" + REPOSITORY + "/" + path,
            data=None if payload is None else json.dumps(payload).encode(),
            method=method,
            headers={
                "Authorization": "Bearer " + token,
                "Accept": "application/vnd.github+json",
                "X-GitHub-Api-Version": "2022-11-28",
                "User-Agent": "SFM-Distribution",
            },
        )
        try:
            with urllib.request.urlopen(request, timeout=30) as response:
                return json.load(response)
        except urllib.error.HTTPError as error:
            if missing and error.code == 404:
                return None
            raise RuntimeError(
                "GitHub HTTP " + str(error.code) + " em " + path
            ) from None


def git(*args):
    return subprocess.check_output(["git", *args], text=True).strip()


def criteria(api, pr_number, sha, comment_id):
    exact_sha(sha)
    pr = api.call(f"pulls/{pr_number}")
    branch = api.call("branches/main")
    comment = api.call(f"issues/comments/{comment_id}")
    require(
        comment.get("issue_url", "").endswith("/issues/" + str(pr_number)),
        "Homologação pertence a outro PR",
    )
    version = git("show", sha + ":VERSION")
    require(bool(re.fullmatch(r"\d+\.\d+\.\d+", version)), "VERSION inválido")
    runs = api.call("actions/runs?head_sha=" + sha + "&per_page=100")["workflow_runs"]
    # A entrada manual em build.yml e o controlador, nao uma validacao concluida.
    # Excluir somente esta execucao; outros checks em curso/falhos continuam bloqueando.
    current_run = os.environ.get("GITHUB_RUN_ID", "")
    runs = [run for run in runs if str(run.get("id")) != current_run]
    report = git("show", sha + f":docs/validacao-console/v{version}.md")
    approval = promotion_gate(
        pr, branch, runs, comment, report, "TemudjinBR", sha, version
    )
    require(
        f"Canal estável: v{version}" in git("show", sha + ":README.md"),
        "README precisa declarar o canal estável desta promoção",
    )
    for path in ("README.md", "CHANGELOG.md", f"release-notes/v{version}.md"):
        require(
            version in git("show", sha + ":" + path),
            "Documento não atualizado: " + path,
        )
    reference = api.call(f"git/ref/tags/v{version}", missing=True)
    if reference:
        require(
            reference.get("object", {}).get("sha") == sha,
            "Tag existente aponta a outro commit; não mover",
        )
    existing = api.call(f"releases/tags/v{version}", missing=True)
    if existing:
        require(
            existing.get("target_commitish") == sha
            and existing.get("prerelease") is False
            and not existing.get("draft"),
            "Número já publicado em outro canal/commit; escolher versão inédita",
        )
    if pr.get("merged"):
        subprocess.run(["git", "fetch", "origin", "main"], check=True)
        require(
            subprocess.run(
                ["git", "merge-base", "--is-ancestor", sha, "FETCH_HEAD"]
            ).returncode
            == 0,
            "PR promovido não alcançável na main",
        )
    Path("approval.json").write_text(json.dumps(approval))
    return approval


def publish(api, sha, channel, pr_number=None, comment_id=None):
    exact_sha(sha)
    version = read_version()
    directory = Path("dist")
    manifest = json.loads((directory / "update.json").read_text())
    require(
        manifest["version"] == version
        and manifest["tag"] == "v" + version
        and manifest["commit"] == sha
        and manifest["channel"] == channel,
        "Manifesto/tag/commit/canal divergem",
    )
    files = asset_names(version)
    require(
        set(p.name for p in directory.iterdir()) == set(files),
        "Arquivos inesperados ou ausentes na distribuição",
    )
    digests = {
        name: hashlib.sha256((directory / name).read_bytes()).hexdigest()
        for name in files
    }
    for key, name in [
        ("app", "SwitchFileManager.nro"),
        ("helper", "SfmUpdateHelper.nro"),
    ]:
        require(
            digests[name] == manifest[key]["sha256"]
            and (directory / name).stat().st_size == manifest[key]["size"],
            "Binário diverge do manifesto",
        )
    existing = api.call("releases/tags/v" + version, missing=True)
    if release_decision(existing, manifest, digests) == "noop":
        print("Release idêntica já publicada; nenhuma alteração.")
        return
    if channel == "stable":
        approval = criteria(api, pr_number, sha, comment_id)
        require(
            approval["nro_sha256"] == digests["SwitchFileManager.nro"]
            and approval["helper_sha256"] == digests["SfmUpdateHelper.nro"],
            "Binários recompilados divergem dos homologados; promoção bloqueada",
        )
        # O endpoint respeita branch protection. Não há push direto ou bypass de revisão/checks.
        pr = api.call(f"pulls/{pr_number}")
        if not pr.get("merged"):
            merged = api.call(
                f"pulls/{pr_number}/merge",
                "PUT",
                {
                    "sha": sha,
                    "merge_method": "merge",
                    "commit_title": f"Promover Switch File Manager v{version} homologado",
                },
            )
            require(
                merged.get("merged") is True, "Merge recusado pela proteção do GitHub"
            )
        subprocess.run(["git", "fetch", "origin", "main"], check=True)
        require(
            subprocess.run(
                ["git", "merge-base", "--is-ancestor", sha, "FETCH_HEAD"]
            ).returncode
            == 0,
            "Commit homologado não alcançável na main",
        )
    else:
        branch = api.call("branches/codex%2Freorganizacao-estabilizacao")
        require(
            branch["commit"]["sha"] == sha,
            "Experimental precisa corresponder à branch de trabalho atual",
        )
    tag = "v" + version
    # Numeração inédita. Nunca mover tag ou usar --clobber.
    reference = api.call("git/ref/tags/" + tag, missing=True)
    if reference:
        require(
            reference.get("object", {}).get("sha") == sha,
            "Tag existente divergente; não mover",
        )
    else:
        api.call("git/refs", "POST", {"ref": "refs/tags/" + tag, "sha": sha})
    notes_file = f"release-notes/{tag}.md"
    if channel == "stable":
        proof = f"https://github.com/{REPOSITORY}/pull/{pr_number}#issuecomment-{comment_id}"
        approved_notes = Path("approved-release-notes.md")
        approved_notes.write_text(
            Path(notes_file).read_text()
            + f"\n\nHomologação e autorização do commit {sha}: {proof}\n"
        )
        notes_file = str(approved_notes)
    command = [
        "gh",
        "release",
        "create",
        tag,
        *[str(directory / name) for name in files],
        "--repo",
        REPOSITORY,
        "--verify-tag",
        "--target",
        sha,
        "--title",
        f"Switch File Manager {tag}"
        + (" — experimental" if channel == "experimental" else ""),
        "--notes-file",
        notes_file,
    ]
    if channel == "experimental":
        command.append("--prerelease")
    subprocess.run(command, check=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["criteria", "publish"])
    parser.add_argument("--sha", required=True)
    parser.add_argument(
        "--channel", choices=["stable", "experimental"], default="experimental"
    )
    parser.add_argument("--pr", type=int)
    parser.add_argument("--approval-comment", type=int)
    args = parser.parse_args()
    require(
        os.environ.get("GITHUB_REPOSITORY") == REPOSITORY, "Repositório não autorizado"
    )
    require(
        os.environ.get("GITHUB_ACTOR") == "TemudjinBR",
        "Somente o proprietário pode acionar distribuição",
    )
    if args.action == "criteria":
        criteria(GitHub(), args.pr, args.sha, args.approval_comment)
    else:
        publish(GitHub(), args.sha, args.channel, args.pr, args.approval_comment)
