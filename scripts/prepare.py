#!/usr/bin/env python3
"""Prepara apenas arquivos gerados. Repetições não alteram fontes ou timestamps."""
from pathlib import Path
from version import ROOT, read_version


def prepare(root=ROOT):
    version = read_version(root)
    generated = root / 'build' / 'generated'
    generated.mkdir(parents=True, exist_ok=True)
    target = generated / 'version.h'
    content = f'#pragma once\n#define SFM_VERSION "{version}"\n'
    if not target.exists() or target.read_text(encoding='utf-8') != content:
        temporary = target.with_suffix('.tmp')
        temporary.write_text(content, encoding='utf-8')
        temporary.replace(target)
    return target


if __name__ == '__main__':
    print('Preparação concluída:', prepare())
