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

## Falha de inicialização relatada no Lite — Missão 3.1

Relato: **FTP indisponível — Falha ao inicializar a rede.** O código anterior chamava `socketInitializeDefault()` tanto no FTP quanto no Drive, com flags de posse independentes. O [libnx 4.12.0](https://github.com/switchbrew/libnx/blob/7644c9b26099aa2d2145bc72a21ee24190e92085/nx/source/runtime/devices/socket.c) recusa a segunda inicialização de `soc:` com `LibnxError_AlreadyInitialized`; `socketExit()` remove esse dispositivo. O conflito de ciclo de vida foi reproduzido na fronteira simulada do SDK. Não há Result do console para afirmar que esse foi o único motivo do relato.

Na 0.10.10, [socket_runtime.cpp](../src/network/socket_runtime.cpp) coordena uma posse idempotente por serviço. O primeiro consumidor inicializa; o último encerra. Parar FTP não encerra sockets do Drive; encerrar Drive não encerra FTP. Falha deixa o consumidor livre para tentar novamente. A inicialização SSL desfaz somente sua própria posse em falha, preservando o FTP.

Falhas de inicialização de sockets registram `Result=0xXXXXXXXX`, módulo/descrição, tipo de applet e memória total/usada quando o SDK permite consultá-las. Logs em `sdmc:/switch/Switch File Manager/logs/sfm.log`, categoria REDE. Result não é errno e não é substituído por código inventado. O transporte de arquivos mantém porta 2121, buffers existentes e aviso anônimo; não há promessa de resolver falta real de memória apenas por unificar os serviços.

Os testes de ciclo de vida exercitam o servidor FTP real em loopback, upload/download, parada/reinício, falha recuperável e Drive → FTP/FTP → Drive. A fronteira libnx simula serviço já inicializado e Result de falha; não simula applet, Wi-Fi, SSL ou memória do console. Validar no Lite em modo applet e memória completa, antes e depois de usar Drive.

## Correcao e diagnostico da Missao 3.2

O anexo atualizado confirma FTP isolado, antes e apos Drive funcionando na 0.10.10; somente o uso simultaneo foi reprovado. O relato inicial foi retificado. A correcao de posse compartilhada continua necessaria, mas nao comprovou a causa exclusiva no hardware e a 3.1 nao foi aprovada para producao.

O codigo anterior descobria IP conectando um socket UDP a 1.1.1.1:53. A nova candidata usa nifmGetCurrentIpAddress e estado local; se NIFM estiver indisponivel, tenta gethostid do BSD ja inicializado, registrando a origem. FTP LAN nao exige acesso a um servidor externo. Isso remove uma dependencia concreta, sem provar a causa do relato fisico.

Falhas agora aparecem na propria tela com etapa e codigo: sockets-inicializar/Result (SDK), wifi-ip/NIFM, criar-socket, configurar-socket, vincular-2121, escutar-2121 e consultar-porta/errno. Threads usam pthread_create com retorno verificado, identificacao pthread e pilha de 256 KiB; nao se depende de excecao de std::thread em build sem excecoes. Os buffers de dados continuam no heap. Falha de cliente recebe 421 e e registrada.

O dialogo de erro oferece diagnostico detalhado; o dialogo ativo tambem permite consulta. Result, errno e pthread sao codigos de origens distintas. O contexto mostra applet/memoria e donos FTP/Drive. Nao se reinicia globalmente sockets enquanto o Drive os possui. Parada/reinicio libera somente os descritores e a posse do FTP.

O worker registra erro de select, e uma nova inicializacao recolhe threads/descritores terminados. Testes simulam queda/retorno de Wi-Fi e falhas em cada etapa, com transferencias FTP reais em loopback. NIFM, Wi-Fi, recursos/threads e modos applet/memoria completa continuam dependentes do Lite. [Checklist](VALIDACAO_SWITCH_LITE_3_2.md).

## Simultaneidade FTP + Drive — 0.10.12

O servidor existente foi mantido: porta 2121, quatro clientes, ativo/passivo e operações anteriores. O SDK libnx 4.12.0 usa três sessões BSD por padrão; cada chamada IPC síncrona ocupa uma sessão até responder. O código anterior podia ocupar essas sessões com recv/select bloqueantes de clientes ociosos, enquanto outro serviço aguardava uma sessão livre. Isso é uma limitação comprovada no código do SDK, **não a causa física comprovada do Lite**.

A candidata solicita oito sessões, mantendo buffers padrão; somente falta explícita de recursos permite fallback para três, identificado na tela. Controle, listener e dados FTP usam O_NONBLOCK, select com timeout zero e espera de 10 ms fora do IPC. A thread do cliente fecha seus próprios descritores; a UI sinaliza parada e aguarda os workers, evitando shutdown sobre um número fechado/reutilizado pelo Drive.

Navegação/envio/download Drive consultam a UI durante espera TCP/TLS. **Y** inicia ou para FTP sem chamar novamente a API Drive; a primeira ativação exige confirmação do acesso total anônimo. **B** solicita cancelamento Drive. Pausar downloads mantém o FTP ativo; voltar o painel ao SD não desconecta conta nem FTP. Os controles não permitem navegar outro painel durante uma transferência remota síncrona.

O diagnóstico FTP é paginado para não ocultar códigos fora da tela e inclui contexto Drive: etapa TCP/SSL, Result, errno, requisições em andamento, parada solicitada, sessões BSD efetivas, donos, tipo de applet/memória quando disponíveis. Não contém autorização ou tokens. Falha real de recursos permanece visível; não se força reinicialização global.

`test_concurrent_network` executa ambos os serviços de produção no **mesmo processo**, mantém a resposta HTTP Drive pendente e só a libera depois de terminar/verificar FTP. Cobre download, upload em múltiplos PUTs, navegação, quatro clientes, parada/reinício FTP e parada Drive durante requisição. Isso prova sobreposição no host POSIX, sem TLS, e não desempenho/estabilidade no libnx. O [checklist físico](VALIDACAO_SWITCH_LITE_3_2.md) testa as duas ordens e modos disponíveis.
