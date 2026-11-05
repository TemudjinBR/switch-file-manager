#!/usr/bin/env python3
"""Lê e valida a única fonte de versão, sem depender de Git ou devkitPro."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def read_version(root=ROOT):
    version = (root / 'VERSION').read_text(encoding='utf-8').strip()
    if not re.fullmatch(r'(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)', version):
        raise ValueError('VERSION deve conter uma versão X.Y.Z válida')
    return version


if __name__ == '__main__':
    print(read_version())
