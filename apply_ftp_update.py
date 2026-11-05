#!/usr/bin/env python3
"""Idempotent source migration for the Switch File Manager FTP v0.10.2 build.
The GitHub connector is currently read-only: this retains a reproducible
source modification on CI without asking the user to edit 77K of main.cpp.
"""
from pathlib import Path
import sys

SOURCE = Path(__file__).resolve().with_name('main.cpp')
s = SOURCE.read_text(encoding='utf-8')
if 'Switch File Manager v0.10.2' in s and 'FTP ATIVO' in s:
    print('FTP v0.10.2: main.cpp already updated')
    sys.exit(0)
if not s.startswith('// Switch File Manager v0.10.0'):
    raise SystemExit('main.cpp does not match the inspected v0.10.0 baseline')

begin = s.index('static void wifiMenu() {')
end = s.index('\nstatic void actionsMenu() {', begin)
new_menu = '''static void wifiMenu() {
    if (!wifi::running()) {
        if (modal("Servidor FTP", "Permitir leitura e gravacao na raiz do cartao SD pela rede local?\\nSem usuario e senha. Use somente Wi-Fi confiavel.", {"Cancelar", "Iniciar FTP"}) != 1) return;
        std::string err;
        if (!wifi::start(err)) { message("FTP indisponivel", err); return; }
    }
    int choice = modal("Servidor FTP ativo",
        "Endereco: " + wifi::url() +
        "\\nRaiz: / (cartao SD completo)" +
        "\\nAcesso anonimo, sem senha" +
        "\\nArquivos transferidos: " + std::to_string(wifi::filesCompleted()) +
        "\\nDados: " + fmtSize(wifi::bytesTransferred()) +
        "\\n\\nConecte usando um gerenciador FTP no celular.",
        {"Manter ativo", "Parar servidor FTP"});
    if (choice == 1) wifi::stop();
}
'''
s = s[:begin] + new_menu + s[end:]
old = '    drawText(fL, "Switch File Manager", 47, 14, C_TXT);'
if s.count(old) != 1:
    raise SystemExit('Could not locate the unique main screen title')
new = old + '''
    if (wifi::running()) {
        std::string ftpStatus = "FTP ATIVO | " + std::to_string(wifi::filesCompleted()) +
            " concluidas | " + fmtSize(wifi::bytesTransferred()) + " transferidos";
        drawText(fS, ftpStatus, 390, 45, C_GRN, 570);
    }'''
s = s.replace(old, new)
s = s.replace('// Switch File Manager v0.10.0', '// Switch File Manager v0.10.2', 1)
SOURCE.write_text(s, encoding='utf-8')
print('FTP v0.10.2: updated main.cpp; menu, address, version and active status')
