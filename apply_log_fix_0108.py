#!/usr/bin/env python3
"""Padroniza logs da v0.10.8, sem duplicar patches a cada build."""
from pathlib import Path

root = Path(__file__).resolve().parent
header, main, ftp = (root / name for name in
                     ('sfm_diag.h', 'main.cpp', 'wifi_transfer.cpp'))
for path in (header, main, ftp):
    if not path.is_file():
        raise SystemExit(f'ERRO v0.10.8: arquivo ausente: {path.name}')

h = header.read_text(encoding='utf-8')
m = main.read_text(encoding='utf-8')
w = ftp.read_text(encoding='utf-8')

# Primeiro o make executa apply_diagnostics.py. Repetir make deve continuar válido.
start7 = 'sfm_diag::write("APP", "Inicio v0.10.7")'
start8 = 'sfm_diag::write("APP", "Inicio v0.10.8")'
required_ftp = 'sfm_diag::write("FTP", msg)'
required_end = 'sfm_diag::write("APP", "Encerramento normal")'
if start7 not in m and start8 not in m:
    raise SystemExit('ERRO: falta instrumentacao de inicializacao (apply_diagnostics.py)')
if required_end not in m or required_ftp not in w:
    raise SystemExit('ERRO: faltam pontos de log de encerramento ou FTP')
if '#include "sfm_diag.h"' not in m or '#include "sfm_diag.h"' not in w:
    raise SystemExit('ERRO: cabeçalho de log nao incluído no codigo')

old = 'sdmc:/switch/SwitchFileManager'
new = 'sdmc:/switch/Switch File Manager'
if old not in h and new not in h:
    raise SystemExit('ERRO: caminho dos logs nao encontrado em sfm_diag.h')
h2 = h.replace(old, new)
m2 = m.replace(start7, start8)
if f'mkdir("{new}/logs"' not in h2 or f'fopen("{new}/logs/sfm.log"' not in h2:
    raise SystemExit('ERRO: a pasta ou o arquivo de log nao estao configurados corretamente')
if start8 not in m2:
    raise SystemExit('ERRO: versao do log de inicializacao nao foi atualizada')

if h2 != h:
    header.write_text(h2, encoding='utf-8')
if m2 != m:
    main.write_text(m2, encoding='utf-8')
print('OK: v0.10.8, log padronizado, verificacoes concluídas')
