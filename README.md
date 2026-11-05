# Switch File Manager — v0.10.5

## Atualização FTP
- Retirado **Listagens** da tela FTP.
- Última atividade registra horário, endereço IP do cliente e eventos de conexão, desconexão, início e conclusão de transferências.
- Retiradas mensagens repetitivas como “Recebendo: 125 MiB”.
- Contadores de arquivos concluídos e total transferido continuam separados.
- Mantida atualização automática das pastas após operações FTP (v0.10.4).
- Versão do Homebrew Menu: **0.10.5**.

**Observação:** o protocolo FTP não fornece necessariamente o nome do dispositivo. A identificação confiável é o IP.

## Histórico
- v0.1.0: estrutura inicial.
- v0.2.0: ícone e interface.
- v0.3.0: seleção múltipla.
- v0.4.0: segurança de arquivos.
- v0.5.0: verificação em movimentações.
- v0.6.0: progresso e cancelamento.
- v0.7.0: tratamento de falhas.
- v0.8.0: fila de transferências (teste confirmado).
- v0.9.0: melhorias do Drive (teste confirmado).
- v0.10.0 a v0.10.4: rede Wi-Fi e FTP.
- v0.10.5: painel FTP simplificado (aguarda compilação e teste).

## Instalação
Substitua o `Makefile` e `README.md` e adicione `apply_ftp_status.py` à raiz. Mantenha os scripts anteriores `apply_ftp_update.py` e `apply_live_refresh.py`. O Makefile executa os três na ordem antes da compilação.

**Segurança:** o FTP atual é anônimo e dá acesso ao SD inteiro. Utilize somente rede confiável e desligue após transferir.
