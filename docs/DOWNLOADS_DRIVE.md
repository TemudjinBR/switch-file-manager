# Downloads recuperaveis — 0.10.11 experimental

## Escopo e relato

O proprietario relatou que a 0.10.10 falhou ao perder internet durante um download e nao retomou. O codigo anterior criava diretamente o destino, fazia tres tentativas imediatas e removia o parcial em falha. A nova maquina de transferencia preserva dados em area tecnica, oferece espera limitada e permite continuar apos reiniciar. Nao implementa retomada de upload.

## Controle pelo aplicativo

Durante o download:

- **X / Pausar**, ou toque no botao: para a requisicao corrente, sincroniza o checkpoint e aguarda, sem novas requisicoes enquanto pausado.
- **X / Continuar**, ou toque: reconsulta identidade/versao antes de acrescentar dados.
- **B / Cancelar**, ou toque: encerra a tarefa e preserva o parcial. Uma tarefa cancelada so volta a transferir por escolha explicita.

A sobreposicao mostra bytes, total, velocidade, tentativa, espera e estado: Conectando, Baixando, Reconectando, Pausado, Reiniciando com seguranca, Verificando, Finalizando, Concluido, Interrompido ou Cancelado. A velocidade zera durante pausa/espera. Bytes de rede podem superar o tamanho final em repeticoes; progresso do arquivo acompanha o offset atual, sem somar retransmissoes como dados novos.

Depois de falha/reinicio, abrir **Acoes → Downloads do Drive**. A lista e da conta atual; cada tarefa oferece Continuar ou Cancelar. A restauracao de autorizacao usa o fluxo existente, sem pedir novo consentimento quando o refresh ainda funciona. Tarefas de outra conta sao recusadas.

## Protocolo de retomada

1. Consultar id, tamanho e version do arquivo, incluindo modifiedTime/md5Checksum como parte da identidade da revisao.
2. Criar parcial exclusivo em diretorio da tarefa e registrar o estado local com gravacao/releitura verificadas.
3. Requisitar Range; validar status, Content-Range, tamanho e limites antes de encaminhar corpo para gravacao.
4. Tratar escrita parcial/EINTR; registrar offset e CRC do prefixo sincronizado a cada MiB e ao terminar/interromper a requisicao.
5. Antes da proxima requisicao ou publicacao, consultar a versao remota novamente. Se mudou, preservar o parcial anterior e iniciar outra geracao, sem juntar dados.
6. Usar If-Range quando um ETag forte de conteudo foi recebido. Uma resposta 200 para offset nao zero, 412 ou 416 provoca reinicio seguro em outro parcial, informado na tela e limitado pelo orcamento de tentativas.
7. Conferir tamanho e CRC local antes de publicar; mover para destino vazio usando operacao que recusa sobrescrita. Destino concorrente/preexistente permanece intacto.

O checkpoint prova integridade local do prefixo e sua associacao a revisao previamente consultada. Se a rede cai antes de nova consulta, o prefixo continua preservado; a identidade remota sera revalidada antes de reutiliza-lo. CRC nao e autenticacao criptografica contra um atacante no SD. A versao monotona e a identidade do Drive impedem a reutilizacao do prefixo de uma revisao diferente; a garantia supoe resposta correta do servico remoto e nao bloqueia modificacao externa dos arquivos tecnicos.

HTTP 401 usa a renovacao existente; respostas de erro nao sao gravadas como arquivo. Intervalo/corpo invalidos nao sao publicados. Falha SD interrompe com etapa/codigo e preserva o checkpoint anterior, sem tratar disco cheio como erro Google.

## Espera e limites

Producao permite cinco repeticoes alem da tentativa inicial, com esperas de 1, 2, 4, 8 e 10 segundos. A UI e consultada durante espera. Pausa voluntaria nao e uma nova tentativa de rede. Ao esgotar o limite, o parcial/journal continuam disponiveis para uma continuacao explicita.

Somente o contexto de downloads usa SSL nao bloqueante: espera ate 15 s no handshake e 5 s por operacao de leitura/escrita, consultando os controles entre retornos WouldBlock. Erros de certificado nao sao ignorados nem transformados em sucesso. O [libnx documenta](https://github.com/switchbrew/libnx/blob/7644c9b26099aa2d2145bc72a21ee24190e92085/nx/include/switch/services/ssl.h) timeout padrao de cinco minutos no modo bloqueante. [Switchbrew](https://www.switchbrew.org/wiki/Error_codes) identifica WouldBlock/timeout/abort do modulo SSL; somente WouldBlock e repetido pelo controlador. DNS/conexao TCP e servicos do console podem ter outros prazos; nao ha promessa de resposta instantanea universal.

## Area tecnica e recuperacao

Diretorio: `sdmc:/switch/switch-manager/downloads/`. O marcador area.txt identifica a area; diretorio desconhecido sem marcador nao e adotado. Subdiretorios usam hash de conta/arquivo/destino. Cada tarefa tem transfer.bin e backups, mais partial-N por geracao. Tokens e client secrets nunca entram no journal. O conteudo baixado e os caminhos podem ser privados: nao compartilhar a area ou inclui-la em commits/artefatos.

Ao carregar um checkpoint, conferir esquema/CRC do journal, conta, id/destino e CRC do prefixo. Um sufixo nao confirmado e descartado somente no parcial da tarefa, antes de continuar. Candidatos de journal incompletos nao viram checkpoints automaticamente. Interrupcao antes do primeiro journal pode iniciar outro parcial, preservando arquivos tecnicos reconhecidos anteriores; conteudo estranho e mantido com erro.

Antes de publicar, o estado Finalizando e persistido. Se o processo parar depois de mover o arquivo, a proxima execucao pode reconhecer o destino completo pelo tamanho/CRC, sem sobrescreve-lo. A recuperacao nao promete atomicidade diante de corte eletrico, corrupcao do controlador ou alteracoes externas simultaneas. Uma interrupcao antes de criar um marcador valido da area ainda pode exigir diagnostico; nao se sobrescreve diretorio pessoal para reparar isso.

Parciais antigos/cancelados e backups de journal nao sao expurgados automaticamente e consomem espaco. Arquivos de uma arvore que ja terminaram ficam preservados em falha de outro item; apenas arquivos efetivamente iniciados tem journal, nao uma fila persistente de toda a arvore. FAT32 continua limitado a menos de 4 GiB por arquivo; arquivos muito grandes/exFAT exigem teste proprio.

## Evidencias e pendencias

Testes usam E/S real temporaria e HTTP ficticio para queda de conexao, Range, mudanca remota, cancelamento, pausa, reinicio, CRC, publicacao concorrente e erros SD. O controle SSL e testado com resultados simulados; isso nao testa TLS real no Lite. [Relatorio](RELATORIO_VALIDACAO.md) e [checklist fisico](VALIDACAO_SWITCH_LITE_3_2.md). A candidata continua experimental.
