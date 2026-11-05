#!/usr/bin/env python3
"""Correção idempotente de destino dos logs depois de apply_diagnostics.py."""
from pathlib import Path
root = Path(__file__).resolve().parent
header = root / 'sfm_diag.h'
main = root / 'main.cpp'
ftp = root / 'wifi_transfer.cpp'
for p in (header, main, ftp):
    if not p.exists():
        raise SystemExit(f'ERRO: arquivo ausente: {p.name}')
h = header.read_text(encoding='utf-8')
m = main.read_text(encoding='utf-8')
w = ftp.read_text(encoding='utf-8')
if 'sfm_diag::write("APP", "Inicio v0.10.7")' not in m or 'sfm_diag::write("FTP", msg)' not in w:
    raise SystemExit('ERRO: apply_diagnostics.py nao aplicou os pontos de log esperados; compilacao interrompida')
old = 'sdmc:/switch/SwitchFileManager'
new = 'sdmc:/switch/Switch File Manager'
if old not in h and new not in h:
    raise SystemExit('ERRO: caminho de log nao reconhecido')
h = h.replace(old, new)
if 'sfm_diag::write("APP", "Inicio v0.10.7")' in m:
    m = m.replace('sfm_diag::write("APP", "Inicio v0.10.7")', 'sfm_diag::write("APP", "Inicio v0.10.8")')
header.write_text(h, encoding='utf-8')
main.write_text(m, encoding='utf-8')
print('Log v0.10.8: caminho unificado e pontos de log verificados')
