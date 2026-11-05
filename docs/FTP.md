# FTP e transferência pela rede local

Estado: **implementado**, com testes de integração no host. Desempenho e estabilidade prolongada no Switch continuam pendentes. A autoridade de implementação é [wifi_transfer.cpp](../src/network/wifi_transfer.cpp); a UI está em `wifiMenu`, `modal` e `drawMain` de [main.cpp](../src/app/main.cpp).

## Conectar

1. Conecte console e cliente à mesma rede local confiável.
2. Abra **X → Ações → Transferência Wi-Fi** e confirme a ativação.
3. No cliente FTP, use o endereço exibido pelo aplicativo, com **porta 2121**. O login é anônimo; não há senha que proteja o acesso.
4. Mantenha o servidor ativo durante a operação. Para encerrar, volte ao mesmo menu e escolha **Parar servidor FTP**; o encerramento do aplicativo também chama a parada.

**Sem senha e sem TLS, com leitura/escrita no cartão SD inteiro.** Não expor em redes públicas ou encaminhar a porta para a internet. O aviso para pastas críticas no menu local não bloqueia modificações pelo FTP.

## Operações implementadas

| Grupo | Comandos e comportamento |
| --- | --- |
| Sessão e informações | USER, PASS, QUIT, NOOP, SYST, FEAT, STAT, PWD/XPWD. |
| Configuração reconhecida | OPTS, TYPE, MODE, STRU, ALLO e CLNT recebem confirmação; isso não prova implementação de todas as semânticas possíveis de cada comando. |
| Navegação | CWD/XCWD e CDUP/XCUP, com resolução dentro da raiz SD. |
| Listagem/metadados | LIST, NLST, MLSD, MLST, SIZE e MDTM. |
| Pastas | MKD/XMKD cria; RMD/XRMD remove pasta vazia. Não é uma remoção recursiva de pasta cheia por um único comando. |
| Arquivos | RETR baixa para o cliente; STOR envia para o SD; DELE exclui arquivo regular. |
| Renomeação | RNFR/RNTO, com verificações de caminho/destino. |
| Conexão de dados | PASV/EPSV e PORT/EPRT IPv4. Alvo ativo precisa corresponder ao IP do cliente e usar porta permitida. |
| Retomada | REST define offset de download. Upload com restart é recusado. |
| ABOR | Reconhecido entre comandos; não prometer interrupção assíncrona instantânea de qualquer transferência bloqueada. |

O código limita as sessões a quatro clientes; testes de carga prolongada e concorrência real no Wi-Fi do Switch não foram concluídos. O total transferido e contadores são da execução do servidor, não um histórico persistente de tráfego.

## O que aparece na interface

O diálogo ativo atualiza endereço, clientes, arquivos concluídos, total de dados transferidos e última atividade. O cabeçalho mostra clientes, listagens e arquivos. Alterações concluídas sinalizadas pelo servidor provocam atualização dos painéis locais; isso também pode limpar marcações por recarregar a lista.

## Integridade e limites

Uploads usam `.sfm-part` com criação exclusiva, flush/fsync e finalização com `.sfm-backup` quando necessário. Temporários existentes não são apagados automaticamente. Uma falha ainda pode exigir conferência manual dos arquivos de teste; não é uma transação completa nem garantia contra perda de energia.

Os testes atuais usam o servidor de produção com sockets POSIX de loopback e um stub limitado ao ciclo de vida de rede libnx. Eles cobrem modos ativo/passivo, upload/download, REST de download, arquivos/pastas, renomeação/exclusão e rejeição de alvos/caminhos inválidos. Não simulam SSL, firmware, cartão SD ou desempenho do console.
