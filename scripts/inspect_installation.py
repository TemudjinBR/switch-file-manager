#!/usr/bin/env python3
"""Inspeção somente leitura de cópias NRO; não remove nem instala arquivos."""
from pathlib import Path
import argparse
import hashlib
import json
from validate_nro import validate
from version import read_version
from package import SD_PATH


def inspect(root, expected=None):
    expected = read_version() if expected is None else expected
    root = Path(root)
    paths = [root] if root.is_file() else sorted(path for path in root.rglob('*') if path.suffix.lower() == '.nro')
    found, invalid = [], []
    for path in paths:
        if path.is_symlink():
            continue
        try:
            metadata = validate(path)
        except (OSError, ValueError, UnicodeError) as error:
            if 'switchfilemanager' in path.name.lower().replace('-', ''):
                invalid.append({'caminho': str(path), 'erro': str(error)})
            continue
        found.append(dict(metadata, caminho=str(path), corresponde=metadata['versao'] == expected,
                          sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
    return {'esperada': expected, 'instalacao_recomendada': SD_PATH,
            'copias': found, 'duplicado': len(found) > 1, 'invalidos': invalid}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('caminho', type=Path)
    args = parser.parse_args()
    print(json.dumps(inspect(args.caminho), ensure_ascii=False, indent=2))
