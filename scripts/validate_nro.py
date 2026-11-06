#!/usr/bin/env python3
"""Valida segmentos NRO, ASET, ícone e metadados NACP, sem executar o aplicativo."""
from pathlib import Path
import argparse
import struct
import re
from prepare import build_identity
from version import read_version


def validate(path, expected_version=None, expected_build=None):
    data = Path(path).read_bytes()
    def require(condition, message):
        if not condition:
            raise ValueError(message)
    require(len(data) >= 0x80 and data[0x10:0x14] == b'NRO0', 'Cabeçalho NRO inválido')
    size = struct.unpack_from('<I', data, 0x18)[0]
    require(0x80 <= size <= len(data), 'Tamanho executável inválido')
    previous = 0
    for offset in (0x20, 0x28, 0x30):
        start, length = struct.unpack_from('<II', data, offset)
        require(length > 0 and start >= previous and start + length <= size,
                'Segmentos NRO inválidos ou sobrepostos')
        previous = start + length
    require(len(data) >= size + 0x38 and data[size:size + 4] == b'ASET', 'ASET ausente')
    assets = []
    for offset in (8, 24, 40):
        start, length = struct.unpack_from('<QQ', data, size + offset)
        if length:
            require(start >= 0x38 and size + start + length <= len(data), 'Asset fora do arquivo')
            assets.append((start, start + length))
    require(all(a[1] <= b[0] for a, b in zip(sorted(assets), sorted(assets)[1:])), 'Assets sobrepostos')
    icon_off, icon_size, nacp_off, nacp_size = struct.unpack_from('<QQQQ', data, size + 8)
    require(icon_size > 2 and data[size + icon_off:size + icon_off + 2] == b'\xff\xd8', 'Ícone JPEG inválido')
    require(nacp_size == 0x4000, 'Tamanho NACP inválido')
    nacp = data[size + nacp_off:size + nacp_off + nacp_size]
    title = nacp[:0x200].split(b'\0', 1)[0].decode('utf-8')
    version = nacp[0x3060:0x3070].split(b'\0', 1)[0].decode('utf-8')
    require(title == 'Switch File Manager', 'Título NACP incorreto')
    if expected_version is not None:
        require(version == expected_version, 'Versão NACP diverge de VERSION')
    markers = set(re.findall(rb'SFM_BUILD:(v[0-9]+\.[0-9]+\.[0-9]+-[0-9a-f]{12})\x00', data[:size]))
    build = next(iter(markers)).decode() if len(markers) == 1 else 'indisponivel'
    if expected_build is not None:
        require(build == expected_build, 'Identificacao do build diverge dos fontes')
    return {'build': build, 'arquivo': Path(path).name, 'bytes': len(data), 'versao': version, 'titulo': title}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('nro', type=Path)
    args = parser.parse_args()
    print(validate(args.nro, read_version(), build_identity()))
