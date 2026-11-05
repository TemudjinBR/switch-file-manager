
# Patch FTP
from pathlib import Path
root=Path(__file__).resolve().parent
src=root/'wifi_transfer.cpp'
main=root/'main.cpp'
s=src.read_text(encoding='utf-8')
m=main.read_text(encoding='utf-8')
marker='// SFM_FTP_PERF_0106'
if marker in s:
    print('FTP v0.10.6 ja otimizado')
    raise SystemExit(0)
assert '// SFM_FTP_STATUS_0105' in m, 'Aplique a versao 0.10.5 antes'
assert s.count('char buf[32768];')==2, 'Buffers FTP nao reconhecidos: abortando sem alterar arquivos'
assert '            slot.data=data;\n            bool ok = data >= 0;' in s, 'Conexao FTP nao reconhecida'
# Blocos maiores diminuem o numero de chamadas de leitura/gravação.
s=s.replace('char buf[32768];','char buf[131072];')
# Ajuste de buffers apenas no socket de dados, nao na conexao de controle.
needle='            slot.data=data;\n            bool ok = data >= 0;'
replacement='''            // SFM_FTP_PERF_0106: buffers de dados maiores, sem alterar o protocolo.
            if (data >= 0) {
                const int socketBuffer = 262144;
                setsockopt(data, SOL_SOCKET, SO_RCVBUF, &socketBuffer, sizeof(socketBuffer));
                setsockopt(data, SOL_SOCKET, SO_SNDBUF, &socketBuffer, sizeof(socketBuffer));
            }
            slot.data=data;
            bool ok = data >= 0;'''
s=s.replace(needle,replacement,1)
# Buffer de E/S para arquivos, liberado automaticamente por fclose.
needle='            restartAt = 0;\n            if (!isListing && !file)'
assert needle in s, 'Abertura de arquivo nao reconhecida'
s=s.replace(needle,'''            restartAt = 0;
            if (file) setvbuf(file, nullptr, _IOFBF, 131072);
            if (!isListing && !file)''',1)
assert s.count('char buf[131072];')==2
assert marker in s
src.write_text(s,encoding='utf-8')
m=m.replace('// Switch File Manager v0.10.5','// Switch File Manager v0.10.6',1)
main.write_text(m,encoding='utf-8')
print('FTP v0.10.6: blocos 128 KiB, socket 256 KiB e buffer de arquivo 128 KiB')
