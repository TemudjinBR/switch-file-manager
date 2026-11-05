#!/usr/bin/env python3
"""Atualização v0.10.5: status FTP, após os patches v0.10.3 e v0.10.4."""
from pathlib import Path
root=Path(__file__).resolve().parent
main=root/'main.cpp'; source=root/'wifi_transfer.cpp'
s=main.read_text(encoding='utf-8'); t=source.read_text(encoding='utf-8')
marker='// SFM_FTP_STATUS_0105'
if marker in s:
    print('v0.10.5 já aplicada'); raise SystemExit(0)
assert '// SFM_FTP_LIVE_0104' in s, 'O patch v0.10.4 deve ser aplicado primeiro'
assert 'void session(int fd, int slotIndex)' in t
assert 'void event(const std::string& msg)' in t
start=s.index('        // SFM_FTP_LIVE_0104:')
end=s.index('        static unsigned lastModalFtpChanges',start)
status=r'''        // SFM_FTP_STATUS_0105: atualização visual em tempo real.
        if (title == "Servidor FTP ativo" && wifi::running()) {
            std::string last = wifi::lastActivity();
            if (last.size() > 62) last = last.substr(0, 59) + "...";
            lines = splitLines(
                "Endereco FTP:\n" + wifi::url() +
                "\nClientes conectados: " + std::to_string(wifi::activeClients()) +
                "\nArquivos concluidos: " + std::to_string(wifi::filesCompleted()) +
                "\nTotal transferido: " + fmtSize(wifi::bytesTransferred()) +
                "\nUltima atividade:\n" + last);
        }
'''
s=s[:start]+status+s[end:]
start=s.index('static void wifiMenu() {');end=s.index('\nstatic void actionsMenu() {',start)
menu=s[start:end]
menu=menu.replace('        "\\nListagens: " + std::to_string(wifi::directoryListings()) +\n','')
menu=menu.replace('        "\\nDados transferidos: " + fmtSize(wifi::bytesTransferred()) +','        "\\nTotal transferido: " + fmtSize(wifi::bytesTransferred()) +')
menu=menu.replace('        "\\nTransferencia atual: " + fmtSize(wifi::currentBytes()) +\n','')
assert 'Listagens:' not in menu
s=s[:start]+menu+s[end:]
s=s.replace('// Switch File Manager v0.10.4','// Switch File Manager v0.10.5',1)
# Evento contém horário, IP do cliente e descrição da operação.
old='void event(const std::string& msg){std::lock_guard<std::mutex> guard(eventLock);lastEvent=msg;}'
assert old in t
t=t.replace(old,'''void event(const std::string& msg){
    time_t now=time(nullptr);
    struct tm local{};
    localtime_r(&now,&local);
    char stamp[12]; strftime(stamp,sizeof(stamp),"%H:%M:%S",&local);
    std::lock_guard<std::mutex> guard(eventLock);
    lastEvent=std::string(stamp)+" - "+msg;
}''',1)
needle='    connectedClients++;\n    event("Celular conectado");'
assert needle in t
t=t.replace(needle,'''    connectedClients++;
    sockaddr_in peer{}; socklen_t peerLen=sizeof(peer);
    std::string device="Cliente FTP";
    if (getpeername(fd,(sockaddr*)&peer,&peerLen)==0) {
        char ip[INET_ADDRSTRLEN]={0};
        if (inet_ntop(AF_INET,&peer.sin_addr,ip,sizeof(ip))) device=ip;
    }
    event("Conectado: "+device);''',1)
needle='    connectedClients--;\n}'
assert needle in t
t=t.replace(needle,'    connectedClients--;\n    event("Desconectado: "+device);\n}',1)
needle='            transferBytes = 0;\n            reply(fd, 150, "Opening data connection");'
assert needle in t
t=t.replace(needle,'''            transferBytes = 0;
            if (op == "STOR") event("Upload iniciado por " + device);
            else if (op == "RETR") event("Download iniciado por " + device);
            reply(fd, 150, "Opening data connection");''',1)
# Substitui a mensagem genérica de conclusão; não exibe MiB em Última atividade.
t=t.replace('event("Transferencia concluida");','event("Arquivo concluido: " + device);')
t=t.replace('event("Erro ao listar ou transferir");','event("Falha na operacao: " + device);')
assert marker in s
assert 'Conectado: ' in t and 'Upload iniciado por ' in t
main.write_text(s,encoding='utf-8');source.write_text(t,encoding='utf-8')
print('v0.10.5 aplicada: status FTP simplificado e cliente identificado por IP')
