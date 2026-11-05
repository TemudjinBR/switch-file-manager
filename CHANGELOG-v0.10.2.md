# Switch File Manager v0.10.2 — FTP experimental

Substitui o HTTP da versão 0.10.0 por FTP passivo local na porta 2121.

- Raiz do servidor: cartão SD completo, apresentado como `/` (`sdmc:/`).
- Sem nome de usuário ou senha: cliente FTP pode usar `anonymous` se exigir preenchimento.
- Upload, download, listagem, criação e remoção de pastas, exclusão e renomeação.
- Upload em arquivo `.sfm-part` seguido de sincronização/renomeação; backup temporário para substituição.
- Navegação segura: não permite escapar do SD usando `..`.
- Mostra endereço e situação do servidor no menu; contador de transferências na tela principal.
- Inicia apenas por comando, para ao selecionar Parar ou ao sair do aplicativo.
- Conexão FTP em modo passivo (PASV/EPSV), sem criptografia, disponível na rede local.
- Suporta comandos USER/PASS, CWD/CDUP, PWD, LIST, NLST, MLSD, TYPE, SIZE, MDTM,
  REST (somente para retomada de download), RETR, STOR, DELE, MKD, RMD, RNFR/RNTO, QUIT e NOOP.

## Limitações e segurança

- Use apenas uma rede Wi-Fi de confiança: qualquer dispositivo que alcançar o servidor
  pode editar ou excluir arquivos do cartão sem autenticação.
- FTP não é HTTP: use gerenciador com suporte FTP (modo passivo) e não navegador web.
- A v0.10.2 suporta uma sessão de controle FTP por vez. Múltiplos arquivos podem ser
  enviados sequencialmente pelo mesmo cliente; transferências paralelas ficam pendentes.
- Retomada de upload ainda não implementada. Não possui criptografia FTP/TLS.
- Não inclui montagem de partições internas, nem gravação em NAND.
- Evite sobrescrever arquivos do Atmosphère ou do bootloader sem backup.
- Servidor FTP foi testado em um executável Linux com stubs das funções do Switch.
  Compilação com devkitA64 e teste em Switch Lite ainda pendentes.

## Empacotamento

O pacote contém `wifi_transfer.cpp`, `wifi_transfer.h`, `Makefile` e
`apply_ftp_update.py` para envio conjunto à raiz do repositório. O `Makefile`
executa automaticamente o patch do `main.cpp` original (v0.10.0) durante a compilação.
O patch é idempotente; a fonte armazenada no GitHub continuará sendo a versão anterior
até o `main.cpp` modificado ser salvo/commitado. Não remova o script dessa versão.
