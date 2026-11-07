#!/usr/bin/env python3
"""Deriva RGB888 nativo do JPEG oficial. Pillow é necessário apenas para regenerar."""
from pathlib import Path
import argparse
import hashlib

ROOT = Path(__file__).resolve().parents[1]
OFFICIAL_SHA = '858ebcb92444859e77ba2485a9532bedda25254ddc00d3fca72517eb5ff38cca'


def generate(root=ROOT):
    from PIL import Image
    source = root / 'assets/icon.jpg'
    if hashlib.sha256(source.read_bytes()).hexdigest() != OFFICIAL_SHA:
        raise ValueError('A arte oficial mudou; revisar explicitamente a origem antes de regenerar')
    with Image.open(source) as image:
        if image.size != (256, 256):
            raise ValueError('A arte deve permanecer em 256x256, sem redimensionamento')
        pixels = image.convert('RGB').tobytes()
    lines = ['#pragma once', '// Gerado por scripts/generate_about_icon.py; não editar à mão.',
             '// JPEG oficial preservado, RGB888 256x256, sem paleta, filtros ou ampliação.',
             '#include <cstdint>', 'namespace sfm::ui::brand {',
             'inline constexpr int kIconSize = 256;',
             '// SHA-256 RGB: ' + hashlib.sha256(pixels).hexdigest(),
             'inline constexpr uint8_t kRgb[] = {']
    for offset in range(0, len(pixels), 24):
        lines.append('    ' + ','.join(str(value) for value in pixels[offset:offset+24]) + ',')
    lines += ['};', 'static_assert(sizeof(kRgb) == 256 * 256 * 3);', '} // namespace sfm::ui::brand', '']
    return '\n'.join(lines)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    header = ROOT / 'include/ui/official_icon.h'
    generated = generate()
    if args.check:
        if header.read_text() != generated:
            raise SystemExit('Ícone derivado diverge da arte oficial')
    else:
        header.write_text(generated)
