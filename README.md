# Switch File Manager — v0.10.6

## FTP — otimização inicial de desempenho
- Blocos de transferência ampliados de 32 KiB para 128 KiB.
- Buffers de socket de dados solicitados em 256 KiB (sujeitos aos limites do sistema).
- Buffer de arquivo de 128 KiB para leitura e gravação.
- Mantidos arquivos temporários, finalização segura e verificação `fsync` no upload.
- Preservados painel FTP v0.10.5 e atualização automática das pastas v0.10.4.
- Versão Homebrew Menu: 0.10.6.

## Histórico do projeto
- v0.1.0–v0.7.0: base, interface, seleção e operações seguras.
- v0.8.0: fila de tarefas (testada).
- v0.9.0: Google Drive (testada).
- v0.10.0–v0.10.4: Wi-Fi e FTP.
- v0.10.5: atividade FTP simplificada (teste confirmado pelo usuário).
- v0.10.6: otimização FTP (pendente compilação e teste no Switch).

## Publicação
Substitua Makefile e README.md na raiz; adicione apply_ftp_performance.py. Preserve os scripts apply_ftp_update.py, apply_live_refresh.py e apply_ftp_status.py. GitHub Actions compilará após executar os patches em ordem.

## Teste recomendado
Use o mesmo arquivo grande, celular, cartão SD, distância do roteador e Wi-Fi 5 GHz. Compare tempo e MB/s no DBI e Switch File Manager. Confirme que o arquivo recebido tem o mesmo tamanho e conteúdo do original.

## Segurança
O servidor FTP é anônimo e tem acesso ao SD inteiro: use somente rede confiável e desligue após a transferência.
