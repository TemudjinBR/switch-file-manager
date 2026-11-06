#!/usr/bin/env python3
"""Cria distribuição para SD sem configurações, tokens ou dados do usuário."""
from pathlib import Path
import argparse
import hashlib
import json
import zipfile
from validate_nro import validate
from version import ROOT, read_version
from prepare import build_identity

NRO_NAME = 'SwitchFileManager.nro'
SD_PATH = 'switch/Switch File Manager/' + NRO_NAME


def package(nro, output, root=ROOT):
    version = read_version(root)
    metadata = validate(nro, version, build_identity(root))
    data = Path(nro).read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    (output / NRO_NAME).write_bytes(data)
    (output / (NRO_NAME + '.sha256')).write_text(f'{digest}  {NRO_NAME}\n', encoding='utf-8')
    manifest = json.dumps({'versao': version, 'nro_sha256': digest, 'build': metadata['build'],
                           'validacao_console': 'pendente', 'caminho_instalacao': SD_PATH}, ensure_ascii=False, indent=2) + '\n'
    readme = ('Switch File Manager v' + version + '\n\n'
              'Extraia a pasta switch na raiz do microSD e abra pelo Homebrew Menu.\n'
              'Executável deste pacote: ' + SD_PATH + '\n'
              'Build: ' + metadata['build'] + '\n'
              'Versão NACP conferida: ' + version + '; SHA-256: ' + digest + '\n'
              'O menu Ações mostra a versão em execução; o log APP registra seu caminho.\n'
              'Se aparecer outra versão, confira o caminho selecionado no Homebrew Menu.\n'
              'Cópias em outros diretórios não são removidas automaticamente.\n'
              'Confirme também o Build em Ações > Versão e diagnóstico do NRO.\n'
              'Se a identidade não corresponder a este manifesto, outro NRO foi aberto.\n'
              'As configurações existentes em switch/switch-manager são preservadas.\n'
              'Este pacote exige validação no console antes de uma Release pública.\n')
    archive = output / f'SwitchFileManager-v{version}.zip'
    with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED) as z:
        for name, content in ((SD_PATH, data), ('LEIA-ME.txt', readme.encode()),
                              ('manifest.json', manifest.encode()),
                              ('SHA256SUMS', f'{digest}  {SD_PATH}\n'.encode())):
            entry = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            entry.external_attr = 0o100644 << 16
            z.writestr(entry, content)
    (output / (archive.name + '.sha256')).write_text(
        f'{hashlib.sha256(archive.read_bytes()).hexdigest()}  {archive.name}\n', encoding='utf-8')
    return archive


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--nro', type=Path, default=ROOT / NRO_NAME)
    parser.add_argument('--output', type=Path, default=ROOT / 'dist')
    args = parser.parse_args()
    print('Distribuição criada:', package(args.nro, args.output))
