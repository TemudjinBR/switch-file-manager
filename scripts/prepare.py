#!/usr/bin/env python3
"""Prepara apenas arquivos gerados. Repetições não alteram fontes ou timestamps."""
from pathlib import Path
import hashlib
from version import ROOT, read_version


def build_identity(root=ROOT):
    root = Path(root)
    paths = [root / 'VERSION', root / 'Makefile', root / 'assets/icon.jpg']
    for directory, patterns in [('src', ('*.cpp', '*.c')), ('include', ('*.h',)),
                                ('third_party', ('*.h', '*.c')), ('scripts', ('*.py', '*.sh')), ('tools', ('*.cpp',))]:
        for pattern in patterns:
            paths.extend((root / directory).rglob(pattern))
    digest = hashlib.sha256(b'SFM_BUILD_SCHEMA_1')
    for path in sorted(set(paths)):
        if path.is_file():
            digest.update(path.relative_to(root).as_posix().encode() + b'\0')
            digest.update(hashlib.sha256(path.read_bytes()).digest())
    return 'v' + read_version(root) + '-' + digest.hexdigest()[:12]


def prepare(root=ROOT):
    version = read_version(root)
    generated = root / 'build' / 'generated'
    generated.mkdir(parents=True, exist_ok=True)
    target = generated / 'version.h'
    content = f'#pragma once\n#define SFM_VERSION "{version}"\n#define SFM_BUILD_ID "{build_identity(root)}"\n'
    if not target.exists() or target.read_text(encoding='utf-8') != content:
        temporary = target.with_suffix('.tmp')
        temporary.write_text(content, encoding='utf-8')
        temporary.replace(target)
    return target


if __name__ == '__main__':
    print('Preparação concluída:', prepare())
