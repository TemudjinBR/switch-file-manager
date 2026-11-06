#!/usr/bin/env python3
"""Cria distribuição para SD sem configurações, tokens ou dados do usuário."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import subprocess
import zipfile
from validate_nro import validate
from version import ROOT, read_version
from prepare import build_identity

NRO_NAME = "SwitchFileManager.nro"
SD_PATH = "switch/Switch File Manager/" + NRO_NAME


def package(nro, output, root=ROOT, helper=None, channel="experimental"):
    if channel not in ("stable", "experimental"):
        raise ValueError("Canal invalido")
    version = read_version(root)
    metadata = validate(nro, version, build_identity(root))
    helper = Path(helper) if helper else Path(nro).with_name("SfmUpdateHelper.nro")
    helper_metadata = validate(
        helper, version, build_identity(root), "Atualizador do Switch File Manager"
    )
    helper_data = helper.read_bytes()
    helper_digest = hashlib.sha256(helper_data).hexdigest()
    data = Path(nro).read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    (output / NRO_NAME).write_bytes(data)
    (output / (NRO_NAME + ".sha256")).write_text(
        f"{digest}  {NRO_NAME}\n", encoding="utf-8"
    )
    (output / "SfmUpdateHelper.nro").write_bytes(helper_data)
    (output / "SfmUpdateHelper.nro.sha256").write_text(
        helper_digest + "  SfmUpdateHelper.nro\n"
    )
    manifest = (
        json.dumps(
            {
                "versao": version,
                "nro_sha256": digest,
                "build": metadata["build"],
                "canal": channel,
                "helper_sha256": helper_digest,
                "validacao_console": (
                    "consultar_aprovacao_da_release"
                    if channel == "stable"
                    else "pendente"
                ),
                "caminho_instalacao": SD_PATH,
            },
            ensure_ascii=False,
            indent=2,
        )
        + "\n"
    )
    readme = (
        "Switch File Manager v" + version + "\n\n"
        "Extraia a pasta switch na raiz do microSD e abra pelo Homebrew Menu.\n"
        "Executável deste pacote: " + SD_PATH + "\n"
        "Build: " + metadata["build"] + "\n"
        "Versão NACP conferida: " + version + "; SHA-256: " + digest + "\n"
        "O menu Ações mostra a versão em execução; o log APP registra seu caminho.\n"
        "Se aparecer outra versão, confira o caminho selecionado no Homebrew Menu.\n"
        "Cópias em outros diretórios não são removidas automaticamente.\n"
        "Confirme também o Build em Ações > Versão e diagnóstico do NRO.\n"
        "Se a identidade não corresponder a este manifesto, outro NRO foi aberto.\n"
        "As configurações existentes em switch/switch-manager são preservadas.\n"
        "Android: extraia primeiro o ZIP de Artifacts e depois SwitchFileManager-v"
        + version
        + ".zip.\n"
        "Feche o aplicativo antes de copiar a pasta switch para a raiz do microSD pelo gerenciador Android.\n"
        "Nao copie a pasta externa do ZIP nem apague google.json, token.txt ou favoritos/historico.\n"
        "Reabra o Homebrew Menu e confira versao, build e Caminho em execucao na mesma copia.\n"
        "Atualizacoes: ESTAVEL e o padrao; EXPERIMENTAL exige escolha explicita.\n"
        "O auxiliar so deve ser iniciado pelo aplicativo para uma transacao verificada.\n"
        "Este pacote exige validação no console antes de uma Release pública.\n"
    )
    archive = output / f"SwitchFileManager-v{version}.zip"
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as z:
        for name, content in (
            (SD_PATH, data),
            ("switch/Switch File Manager/SfmUpdateHelper.nro", helper_data),
            ("LEIA-ME.txt", readme.encode()),
            ("manifest.json", manifest.encode()),
            (
                "SHA256SUMS",
                f"{digest}  {SD_PATH}\n{helper_digest}  switch/Switch File Manager/SfmUpdateHelper.nro\n".encode(),
            ),
        ):
            entry = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            entry.external_attr = 0o100644 << 16
            z.writestr(entry, content)
    (output / (archive.name + ".sha256")).write_text(
        f"{hashlib.sha256(archive.read_bytes()).hexdigest()}  {archive.name}\n",
        encoding="utf-8",
    )
    commit = os.environ.get("SFM_SOURCE_COMMIT", "0" * 40)
    if not re.fullmatch("[0-9a-f]{40}", commit):
        raise ValueError("SFM_SOURCE_COMMIT deve ser um SHA completo")
    notes_path = Path(root) / "release-notes" / f"v{version}.md"
    notes = (
        notes_path.read_text()[:4000]
        if notes_path.exists()
        else "Candidata experimental; validacao fisica pendente."
    )
    base = f"https://github.com/TemudjinBR/switch-file-manager/releases/download/v{version}/"
    update = {
        "schema": 1,
        "min_updater": 1,
        "channel": channel,
        "version": version,
        "tag": "v" + version,
        "commit": commit,
        "build": metadata["build"],
        "notes": notes,
        "app": {"url": base + NRO_NAME, "size": len(data), "sha256": digest},
        "helper": {
            "url": base + "SfmUpdateHelper.nro",
            "size": len(helper_data),
            "sha256": helper_digest,
        },
    }
    (output / "update.json").write_text(
        json.dumps(update, ensure_ascii=False, indent=2) + "\n"
    )
    return archive


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--nro", type=Path, default=ROOT / NRO_NAME)
    parser.add_argument("--output", type=Path, default=ROOT / "dist")
    parser.add_argument(
        "--channel", choices=["stable", "experimental"], default="experimental"
    )
    args = parser.parse_args()
    print("Distribuição criada:", package(args.nro, args.output, channel=args.channel))
