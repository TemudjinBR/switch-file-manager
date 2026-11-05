# Switch File Manager

Gerenciador de arquivos para Nintendo Switch com Atmosphère. Interface de dois painéis, operações locais, Google Drive e FTP.

## Versão v0.10.4 — atualização automática do FTP

- Atualização dos contadores de clientes, listagens, arquivos concluídos e bytes enquanto a janela FTP está aberta.
- Atualização automática dos dois painéis locais após envio, exclusão, renomeação ou criação de pasta pelo FTP.
- Preserva o diretório atual: não é necessário sair e entrar novamente para enxergar o arquivo.
- Versão 0.10.4 no Homebrew Menu.
- FTP continua em `ftp://IP_DO_SWITCH:2121`, com acesso anônimo ao SD inteiro. Use somente rede confiável e desligue após transferir.

## Histórico
- v0.1.0: base e compilação.
- v0.2.0: ícone e interface.
- v0.3.0: seleção múltipla.
- v0.4.0: proteção em operações.
- v0.5.0: verificação em movimentações.
- v0.6.0: progresso e cancelamento.
- v0.7.0: tratamento de falhas e parciais.
- v0.8.0: fila de transferências.
- v0.9.0: pré-varredura do Google Drive.
- v0.10.0: Wi-Fi experimental.
- v0.10.1: correção de endereço.
- v0.10.2–v0.10.3: servidor FTP e indicadores.
- v0.10.4: atualização automática de pastas e indicadores FTP.

As versões até v0.9.0 tiveram testes confirmados pelo proprietário. O FTP foi testado funcionalmente pelo proprietário, que identificou a ausência de atualização visual. A v0.10.4 aguarda compilação e teste.

## Compilação

O Makefile aplica primeiro `apply_ftp_update.py` e depois `apply_live_refresh.py`, antes de compilar o projeto. Mantenha os dois scripts na raiz.

## Segurança

O FTP atual permite acesso anônimo de leitura e escrita ao cartão SD completo, sem criptografia. Não utilize redes públicas e desligue o servidor após o uso. Não faça testes destrutivos com arquivos importantes.
