# Gerenciamento local, persistência e espaço

Estado: **implementado**, com testes de arquivos no host; integração da interface e do SD no console ainda precisa de validação específica. Fontes: [operações](../src/storage/operations.cpp), [filesystem](../src/storage/filesystem.cpp), [políticas](../src/security/path_policy.cpp) e [aplicação](../src/app/main.cpp).

## Arquivos e pastas

O painel ativo é origem, e o painel oposto é destino nas operações de cópia/movimentação. As ações usam a seleção marcada; se ela estiver vazia, usam o item em foco. Cópia, movimentação, exclusão, ZIP e propriedades aceitam conjuntos nos fluxos descritos. Renomear trabalha com o item em foco.

- **Copiar:** percorre pastas, cria saída com nome disponível e recusa um destino existente na criação protegida. Não compara byte a byte toda cópia por padrão.
- **Mover local:** tenta `rename`; se não puder, usa cópia, comparação integral e só então remove a origem. Remoção pode falhar parcialmente; não existe rollback de tudo que já ocorreu.
- **Renomear/criar pasta:** valida nomes, usa teclado de sistema e informa conflitos/erros. Não há promessa de bloqueio transacional contra alterações simultâneas.
- **Excluir:** pede confirmação, alerta adicionalmente para os caminhos críticos reconhecidos e informa remoção incompleta. As confirmações da UI não são uma política global aplicada ao FTP.
- **Cancelamento:** B sinaliza às operações que consultam o progresso. A origem é preservada quando o caminho de cópia falha; a limpeza tenta atingir a saída criada pela própria operação. Dados já removidos não são restaurados automaticamente.

A UI impede a cópia de uma pasta para dentro dela mesma e bloqueia mover SD ↔ Drive. Nomes incompatíveis com FAT/exFAT são tratados; pastas críticas incluem os caminhos de atmosphere, bootloader, emuMMC, Nintendo e switch reconhecidos pelo código. Aviso não é proibição absoluta de exclusão.

## Marcação, ordenação e busca

“Visível” no menu de seleção significa a lista filtrada inteira, não apenas as dez linhas aproximadas da área na tela. Marcar somente arquivos ou somente pastas **acrescenta** os tipos indicados; não desmarca automaticamente o outro tipo. Use Limpar todas as marcações antes para obter uma seleção exclusiva.

A ordenação aceita nome, tamanho, data e tipo/extensão, com inversão e desempate por nome. Pastas permanecem antes dos arquivos. A busca é um filtro por trecho do nome na pasta atual; não varre subpastas e não usa normalização Unicode completa. Atualizar, ordenar ou reconstruir a lista limpa marcações; subir/entrar em uma pasta limpa o filtro.

## O que fica salvo

| Dado | Persistência e limite |
| --- | --- |
| Favoritos | Pastas locais em `sdmc:/switch/switch-manager/favoritos.txt`; adicionar, visitar e remover. Drive não pode ser favorito aqui. |
| Histórico | Até 15 diretórios locais recentes em `historico.txt`, sem a raiz; remove repetições e permite limpar com confirmação. |
| Credencial e token Drive | `google.json` e `token.txt` legados preservados. `drive-account.json` guarda autorização/estado atual e preferência; validação física ainda pendente. |
| Desconexão da conta | `enabled=false` em `drive-account.json` bloqueia restauração automática. O marcador `login.disabled` antigo é lido somente na importação não destrutiva. |
| Cópias de recuperação OAuth | `token.txt.sfm-old-NNN` e `google.json.sfm-old-NNN`; temporários `.sfm-new-NNN` não são aceitos como login. Nunca incluir esses arquivos em logs/artefatos. |
| Ordenação e inversão | Por painel, durante a sessão; não são salvas em arquivo. |
| Filtro e marcações | Durante a sessão, com limpeza nas transições/reloads descritos; não são salvos. |
| Fila | Até 20 tarefas locais em memória; não persiste após fechar. |
| Painéis e caminhos abertos | Estado de execução; não há restauração persistente de toda a sessão. |
| Logs | Caminho separado `sdmc:/switch/Switch File Manager/logs/`; um backup de rotação. |

Os caminhos legados são preservados. Esta missão não migra, lê dados privados nem apaga configurações. O salvamento de favoritos/histórico ainda não tem a mesma propagação detalhada de erros das operações de cópia: não se deve prometer persistência garantida diante de falha do SD.

## Fila local

Cada tarefa registra origem, destino, itens escolhidos e se é cópia ou movimentação. Adicionar não modifica arquivos. O limite de 20 é de **tarefas**, não de arquivos. Não existe fila de Drive nesta revisão.

Executar processa em ordem e restaura os painéis ao terminar. Falha/cancelamento mantém a tarefa interrompida e as posteriores. Como uma tarefa pode conter itens já processados, conferir o resultado é necessário antes de repetir. Limpar a fila descarta apenas os registros pendentes, após confirmação; não apaga os arquivos.

## Propriedades e análise de espaço

O cabeçalho consulta `statvfs` para total/livre e calcula o uso visual; a atualização periódica é por quantidade de frames, não por um intervalo fixo garantido em segundos.

Propriedades locais mostram tipo, tamanho em unidades legíveis, bytes adicionais quando aplicável, local e data disponível. Para pastas/conjuntos, calculam arquivos e subpastas. O cálculo de propriedades para de aprofundar depois do limite de 64 níveis; itens com `stat` indisponível e consultas que retornem lista vazia podem reduzir o total sem um relatório completo de omissões. Valores não são uma auditoria exaustiva de uso físico do filesystem.

O analisador mede os itens imediatamente dentro da pasta, incluindo o conteúdo das subpastas calculadas, e apresenta até nove maiores, com percentual do total encontrado. Ele não é um mapa global do cartão. Escolher um item limpa o filtro e o focaliza.

Drive não tem total de pasta calculado nesse diálogo. A listagem remota atual não busca `modifiedTime`; portanto não há promessa de datas remotas disponíveis para ordenar/exibir.

O cartão FAT32 limita cada arquivo a menos de 4 GiB. ZIP64, exFAT, arquivos grandes, perda de energia e alterações concorrentes exigem testes próprios no hardware. Não há indicadores de relógio ou bateria implementados nesta interface.

## Persistência Drive na 0.10.10

A camada específica de credenciais não usa substituição POSIX de arquivo por `rename` sobre destino existente. Ela verifica diretórios, grava um temporário exclusivo sem buffer stdio, trata escritas parciais/EINTR, chama fsync, fecha e relê. O anterior vai para backup livre antes da instalação do novo; falha posterior provoca tentativa de rollback, sem apagar as cópias de dados. Se o canônico estiver ausente depois de interrupção, a leitura recupera o backup anterior mais recente reconhecido.

O login só é confirmado após verificar novamente o token final. Dados de autenticação não são apagados em `invalid_grant`, erros de rede ou falhas do SD. Os slots são descobertos no diretório e numerados em 64 bits, sem o limite artificial de 128. Backups não são expurgados automaticamente e ainda consomem espaço. Esquecer exige confirmação e remove apenas autorizações atribuídas à conta, preservando dados pessoais e a credencial compartilhada do aplicativo. Detalhes e limites em [Persistência Drive](GOOGLE_DRIVE_PERSISTENCIA.md).
