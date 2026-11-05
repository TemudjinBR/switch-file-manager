#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parent
main=root/'main.cpp'; source=root/'wifi_transfer.cpp'; header=root/'wifi_transfer.h'
s=main.read_text(); w=source.read_text(); h=header.read_text()
marker='// SFM_FTP_LIVE_0104'
if marker in s:
    print('Patch v0.10.4 já aplicado'); raise SystemExit(0)
assert 'static void wifiMenu()' in s and 'Servidor FTP ativo' in s, 'Execute apply_ftp_update.py primeiro'
assert 'unsigned directoryListings(){return listingsDone.load();}' in w
assert 'unsigned directoryListings();' in h
assert '        drawMain();\n        fillRect(0, 0, W, H, Col{0, 0, 0, 175});' in s
assert '            if (++frame % 300 == 0) refreshSpace();' in s
w=w.replace('std::atomic<unsigned> listingsDone{0}, connectedClients{0};',
            'std::atomic<unsigned> listingsDone{0}, connectedClients{0};\nstd::atomic<unsigned> changesDone{0};')
w=w.replace('if (path != ROOT && parentExists(path) && mkdir(path.c_str(), 0777) == 0) reply(fd, 257, "\\"' ,
            'if (path != ROOT && parentExists(path) && mkdir(path.c_str(), 0777) == 0) { changesDone++; reply(fd, 257, "\\"')
w=w.replace(' + "\\" created");\n            else reply(fd, 550, "Create failed");',
            ' + "\\" created"); }\n            else reply(fd, 550, "Create failed");')
w=w.replace('if (path != ROOT && rmdir(path.c_str()) == 0) reply(fd, 250, "Directory removed");',
            'if (path != ROOT && rmdir(path.c_str()) == 0) { changesDone++; reply(fd, 250, "Directory removed"); }')
w=w.replace('if (path != ROOT && isRegular(path) && unlink(path.c_str()) == 0) reply(fd, 250, "File deleted");',
            'if (path != ROOT && isRegular(path) && unlink(path.c_str()) == 0) { changesDone++; reply(fd, 250, "File deleted"); }')
w=w.replace('rename(pendingRename.c_str(), path.c_str()) == 0) reply(fd, 250, "Renamed");',
            'rename(pendingRename.c_str(), path.c_str()) == 0) { changesDone++; reply(fd, 250, "Renamed"); }')
w=w.replace('if (ok) ok = finalizeUpload(path, temporary);',
            'if (ok) { ok = finalizeUpload(path, temporary); if (ok) changesDone++; }')
w=w.replace('unsigned directoryListings(){return listingsDone.load();}',
            'unsigned directoryListings(){return listingsDone.load();}\nunsigned fileChanges(){return changesDone.load();}')
w=w.replace('listingsDone=0;connectedClients=0;', 'listingsDone=0;connectedClients=0;changesDone=0;')
h=h.replace('unsigned directoryListings();','unsigned directoryListings();\nunsigned fileChanges();')
needle='        drawMain();\n        fillRect(0, 0, W, H, Col{0, 0, 0, 175});'
replacement='''        // SFM_FTP_LIVE_0104: atualizar o modal enquanto ele estiver aberto.
        if (title == "Servidor FTP ativo" && wifi::running()) {
            std::string last = wifi::lastActivity();
            if (last.size() > 38) last = last.substr(0, 35) + "...";
            lines = splitLines(
                "Endereco FTP:\\\\n" + wifi::url() +
                "\\\\nRaiz: / (cartao SD completo)" +
                "\\\\nAcesso anonimo, sem senha" +
                "\\\\nClientes conectados: " + std::to_string(wifi::activeClients()) +
                "\\\\nListagens: " + std::to_string(wifi::directoryListings()) +
                "\\\\nArquivos concluidos: " + std::to_string(wifi::filesCompleted()) +
                "\\\\nDados transferidos: " + fmtSize(wifi::bytesTransferred()) +
                "\\\\nUltima atividade:\\\\n" + last);
        }
        static unsigned lastModalFtpChanges = 0;
        unsigned modalFtpChanges = wifi::fileChanges();
        if (modalFtpChanges != lastModalFtpChanges) {
            lastModalFtpChanges = modalFtpChanges;
            for (int pi = 0; pi < 2; ++pi)
                if (!panes[pi].drive) panes[pi].reload();
        }
        drawMain();
        fillRect(0, 0, W, H, Col{0, 0, 0, 175});'''
# replacement above contains \\n in generated C++ (must be \n escape, not two literal backslashes)
replacement=replacement.replace('\\\\n','\\n')
s=s.replace(needle,replacement,1)
s=s.replace('            if (++frame % 300 == 0) refreshSpace();',
'''            static unsigned lastFtpChanges = 0;
            unsigned ftpChanges = wifi::fileChanges();
            if (ftpChanges != lastFtpChanges) {
                lastFtpChanges = ftpChanges;
                for (int pi = 0; pi < 2; ++pi)
                    if (!panes[pi].drive) panes[pi].reload();
                refreshSpace();
            }
            if (++frame % 300 == 0) refreshSpace();''',1)
s=s.replace('// Switch File Manager v0.10.3','// Switch File Manager v0.10.4',1)
assert 'changesDone++; reply(fd, 250, "File deleted")' in w
assert 'changesDone++; reply(fd, 250, "Renamed")' in w
assert 'changesDone++; reply(fd, 257,' in w
assert 'if (ok) changesDone++;' in w
assert marker in s
main.write_text(s);source.write_text(w);header.write_text(h)
print('Patch FTP v0.10.4 aplicado')
