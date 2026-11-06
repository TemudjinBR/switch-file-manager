#!/usr/bin/env python3
"""Gates verificáveis de promoção; nenhum teste simulado equivale a homologação."""
import json
import re

REPOSITORY = "TemudjinBR/switch-file-manager"
ASSETS = (
    "SwitchFileManager.nro",
    "SwitchFileManager.nro.sha256",
    "SfmUpdateHelper.nro",
    "SfmUpdateHelper.nro.sha256",
    "update.json",
)


def require(value, message):
    if not value:
        raise ValueError(message)


def exact_sha(value):
    require(
        bool(re.fullmatch("[0-9a-f]{40}", value)) and value != "0" * 40,
        "SHA completo e não nulo obrigatório",
    )
    return value


def approval_record(comment, owner, sha, version):
    require(
        comment.get("user", {}).get("login") == owner,
        "Homologação deve ser registrada pelo proprietário",
    )
    body = comment.get("body", "")
    require(body.startswith("HOMOLOGACAO_SFM_1\n"), "Aprovação estruturada ausente")
    approval = json.loads(body.split("\n", 1)[1])
    require(
        approval.get("commit") == exact_sha(sha) and approval.get("version") == version,
        "Aprovação não corresponde ao commit/versão exatos",
    )
    for field in ("reviewed", "physical", "authorize_merge_release"):
        require(approval.get(field) is True, "Gate explícito ausente: " + field)
    for field in ("nro_sha256", "helper_sha256"):
        require(
            bool(re.fullmatch("[0-9a-f]{64}", approval.get(field, ""))),
            "Hash físico inválido: " + field,
        )
    return approval


def promotion_gate(pr, branch, runs, comment, report, owner, sha, version):
    exact_sha(sha)
    require(
        (
            pr.get("state") == "open"
            and not pr.get("merged")
            or pr.get("state") == "closed"
            and pr.get("merged") is True
        )
        and not pr.get("draft"),
        "PR precisa estar aberto/revisável ou já ter sido promovido",
    )
    require(
        pr.get("head", {}).get("sha") == sha
        and pr.get("head", {}).get("repo", {}).get("full_name") == REPOSITORY,
        "Head/repositório do PR divergem",
    )
    require(pr.get("base", {}).get("ref") == "main", "Base do PR deve ser main")
    require(branch.get("protected") is True, "main sem proteção: promoção bloqueada")
    checks = branch.get("protection", {}).get("required_status_checks", {})
    require(
        checks.get("enforcement_level") == "everyone"
        and {"tests", "build"} <= set(checks.get("contexts", [])),
        "Proteção precisa exigir tests/build para todos, inclusive administradores",
    )
    valid = [
        run
        for run in runs
        if run.get("head_sha") == sha
        and run.get("name") == "Compilar e testar Switch File Manager"
    ]
    require(
        valid and max(valid, key=lambda r: r["id"]).get("conclusion") == "success",
        "CI mais recente do commit exato não aprovado",
    )
    approval = approval_record(comment, owner, sha, version)
    for text in (
        "Resultado: APROVADO",
        "Dispositivo: Nintendo Switch Lite",
        "Atmosphere:",
        "Formato SD:",
        approval["nro_sha256"],
        approval["helper_sha256"],
    ):
        require(text in report, "Relatório físico incompleto: " + text)
    require(
        "PENDENTE" not in report and "NAO EXECUTADO" not in report,
        "Relatório ainda contém casos pendentes",
    )
    return approval


def asset_names(version):
    return (
        *ASSETS,
        f"SwitchFileManager-v{version}.zip",
        f"SwitchFileManager-v{version}.zip.sha256",
    )


def release_decision(existing, manifest, local_digests):
    """Reexecução idêntica é no-op; colisão ou troca de canal nunca sobrescreve."""
    if existing is None:
        return "create"
    require(existing.get("tag_name") == manifest["tag"], "Tag existente diverge")
    require(
        existing.get("target_commitish") == manifest["commit"],
        "Release existente aponta a outro commit",
    )
    require(
        existing.get("prerelease") == (manifest["channel"] == "experimental")
        and not existing.get("draft"),
        "Número já usado em outro canal/estado; não promover ou sobrescrever implicitamente",
    )
    assets = {asset["name"]: asset for asset in existing.get("assets", [])}
    require(
        len(assets) == len(existing.get("assets", [])),
        "Assets duplicados; nao publicar",
    )
    require(
        set(assets) == set(local_digests),
        "Release existente tem conjunto de arquivos diferente/incompleto; revisão necessária",
    )
    for name, digest in local_digests.items():
        require(
            assets[name].get("digest") == "sha256:" + digest,
            "Asset existente não é idêntico: " + name,
        )
    return "noop"
