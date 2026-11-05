#!/usr/bin/env python3
"""Idempotent migration of Switch File Manager main.cpp for FTP v0.10.3.
Executed before compilation by Makefile. Supports the upstream v0.10.0
and previous v0.10.2 code with no manual changes to main.cpp.
"""
from pathlib import Path
import sys

SOURCE = Path(__file__).resolve().with_name('main.cpp')
if not SOURCE.exists():
    raise SystemExit('main.cpp ausente; publique os arquivos na raiz do projeto')
s = SOURCE.read_text(encoding='utf-8')
if 'Switch File Manager v0.10.3' in s and 'wifi::directoryListings()' in s:
    print('FTP v0.10.3: main.cpp ja atualizado')
    sys.exit(0)
if not (s.startswith('// Switch File Manager v0.10.0') or
        s.startswith('// Switch File Manager v0.10.2')):
    raise SystemExit('Versao desconhecida de main.cpp; cancelado para proteger o codigo')
start = s.index('static void wifiMenu() {')
end = s.index('\nstatic void actionsMenu() {', start)
new_menu = r'''static void wifiMenu() {
    if (!wifi::running()) {
        if (modal("Servidor FTP",
                  "Ativar acesso total ao cartao SD?\n"
                  "Leitura e gravacao, sem senha.\n"
                  "Use somente no Wi-Fi confiavel.\n"
                  "Desligue quando terminar.",
                  {"Cancelar", "Iniciar FTP"}) != 1) return;
        std::string err;
        if (!wifi::start(err)) { message("FTP indisponivel", err); return; }
    }
    std::string last = wifi::lastActivity();
    if (last.size() > 38) last = last.substr(0, 35) + "...";
    int choice = modal("Servidor FTP ativo",
        "Endereco FTP:\n" + wifi::url() +
        "\nRaiz: / (cartao SD completo)" +
        "\nAcesso anonimo, sem senha" +
        "\nClientes conectados: " + std::to_string(wifi::activeClients()) +
        "\nListagens: " + std::to_string(wifi::directoryListings()) +
        "\nArquivos concluidos: " + std::to_string(wifi::filesCompleted()) +
        "\nDados transferidos: " + fmtSize(wifi::bytesTransferred()) +
        "\nUltima atividade:" +
        "\n" + last,
        {"Manter ativo", "Parar servidor FTP"});
    if (choice == 1) wifi::stop();
}
'''
s = s[:start] + new_menu + s[end:]
# Replace only the single top status line inserted in v0.10.2.
import re
s = re.sub(r'    if \(wifi::running\(\)\) \{\n        std::string ftpStatus =.*?\n    \}\n', '', s, count=1, flags=re.S)
old = '    drawText(fL, "Switch File Manager", 47, 14, C_TXT);'
if s.count(old) != 1:
    raise SystemExit('Titulo principal nao encontrado; nenhuma alteracao aplicada')
new = old + r'''
    if (wifi::running()) {
        std::string ftpStatus = "FTP ATIVO | " +
            std::to_string(wifi::activeClients()) + " clientes | " +
            std::to_string(wifi::directoryListings()) + " listagens | " +
            std::to_string(wifi::filesCompleted()) + " arquivos";
        drawText(fS, ftpStatus, 405, 45, C_GRN, 585);
    }
'''
s = s.replace(old, new, 1)
s = s.replace('// Switch File Manager v0.10.0', '// Switch File Manager v0.10.3', 1)
s = s.replace('// Switch File Manager v0.10.2', '// Switch File Manager v0.10.3', 1)
SOURCE.write_text(s, encoding='utf-8')
print('FTP v0.10.3: menu curto, status de clientes/listagens, versao atualizada')
