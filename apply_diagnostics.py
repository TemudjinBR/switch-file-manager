#!/usr/bin/env python3
from pathlib import Path
p=Path(__file__).resolve().parent
main=p/'main.cpp'; wifi=p/'wifi_transfer.cpp'
s=main.read_text(encoding='utf-8'); w=wifi.read_text(encoding='utf-8')
if '// SFM_DIAG_0107' in w:
    print('v0.10.7 ja aplicada');raise SystemExit(0)
assert '// SFM_FTP_PERF_0106' in w
assert w.count('char buf[131072];')==2
w=w.replace('char buf[131072];','std::vector<char> buf(131072);')
w=w.replace('fread(buf, 1, sizeof(buf), file)','fread(buf.data(), 1, buf.size(), file)')
w=w.replace('writeAll(fd, buf, size)','writeAll(fd, buf.data(), size)')
w=w.replace('recv(fd, buf, sizeof(buf), 0)','recv(fd, buf.data(), buf.size(), 0)')
w=w.replace('fwrite(buf + index, 1, remaining, file)','fwrite(buf.data() + index, 1, remaining, file)')
w=w.replace('#include "wifi_transfer.h"','#include "wifi_transfer.h"\n#include "sfm_diag.h" // SFM_DIAG_0107',1)
assert 'lastEvent=std::string(stamp)+" - "+msg;' in w
w=w.replace('lastEvent=std::string(stamp)+" - "+msg;', 'lastEvent=std::string(stamp)+" - "+msg;\n    sfm_diag::write("FTP", msg);',1)
w=w.replace('bool writeFile(int fd, FILE* file) {','bool writeFile(int fd, FILE* file) {\n    sfm_diag::write("FTP", "Inicio download");',1)
w=w.replace('bool receiveFile(int fd, FILE* file) {','bool receiveFile(int fd, FILE* file) {\n    sfm_diag::write("FTP", "Inicio upload");',1)
w=w.replace('        if (count < 0) return false;', '        if (count < 0) { sfm_diag::write("ERRO", "recv errno=" + std::to_string(errno)); return false; }',1)
w=w.replace('            if (!n) return false;', '            if (!n) { sfm_diag::write("ERRO", "fwrite errno=" + std::to_string(errno)); return false; }',1)
assert 'reply(fd, 226, "Transfer complete");' in w
w=w.replace('reply(fd, 226, "Transfer complete");', 'sfm_diag::write("FTP", "Concluido: " + op + " " + path); reply(fd, 226, "Transfer complete");',1)
assert 'reply(fd, 426, "Transfer interrupted or failed");' in w
w=w.replace('reply(fd, 426, "Transfer interrupted or failed");', 'sfm_diag::write("ERRO", "Falha: " + op + " " + path + " errno=" + std::to_string(errno)); reply(fd, 426, "Transfer interrupted or failed");',1)
assert '#include <switch.h>' in s
s=s.replace('#include <switch.h>','#include <switch.h>\n#include "sfm_diag.h" // SFM_DIAG_0107',1)
assert '    (void)argv;\n' in s
s=s.replace('    (void)argv;\n','    (void)argv;\n    sfm_diag::write("APP", "Inicio v0.10.7");\n',1)
assert '    wifi::stop();\n    gd::netExit();' in s
s=s.replace('    wifi::stop();\n    gd::netExit();','    sfm_diag::write("APP", "Encerramento normal");\n    wifi::stop();\n    gd::netExit();',1)
assert '    int r = modal("Ações", "", o);' in s
s=s.replace('    int r = modal("Ações", "", o);','    int r = modal("Ações", "", o);\n    if (r >= 0 && r < (int)o.size()) sfm_diag::write("ACAO", o[r]);',1)
s=s.replace('// Switch File Manager v0.10.6','// Switch File Manager v0.10.7',1)
main.write_text(s,encoding='utf-8');wifi.write_text(w,encoding='utf-8')
print('v0.10.7 aplicada')
