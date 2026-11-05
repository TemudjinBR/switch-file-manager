# Inventário completo de funcionalidades

Revisão inventariada: **0.10.8 experimental**, código herdado do commit `8be971079763e6c74bb3b6f37f1e25d3b976e35f` no PR #1. A Missão 2 altera apresentação e documentação, sem alterar o C++ ou os recursos do NRO. O [catálogo para o usuário](../README.md#funcionalidades) apresenta os recursos; este documento permite conferi-los no código.

## Como ler os estados

- **Implementado:** existe no código com acionamento correspondente. Não significa estabilidade comprovada no console.
- **Experimental:** existe, mas possui falhas/limitações ou validação insuficiente; o Drive inteiro deve ser apresentado assim.
- **Planejado:** ainda indisponível. Fica no roadmap, sem ser contado no catálogo implementado.
- **Validação:** host automatizado, compilação devkitPro, teste físico e pendência física são estados separados. Testes da biblioteca não cobrem automaticamente toda a UI.

## Categorias, evidências e limites

| Categoria / estado | Implementação que deve ser examinada |
| --- | --- |
| Interface e navegação — implementado | [main.cpp](../src/app/main.cpp): `Pane`, `drawPane`, `drawMain`, `movePane`, `handleInput`, `modal`; [renderer.cpp](../src/ui/renderer.cpp). Dois painéis independentes, pastas antes dos arquivos, foco, rolagem, toque, navegação por páginas, nomes, tamanhos e data local no modo de ordenação correspondente. |
| Arquivos/pastas — implementado | `doCopyMove`, `doRename`, `doMkdir`, `doDelete`, `selection` em main; [operations.cpp](../src/storage/operations.cpp). Cópias, movimentação local com fallback verificado, criação, renomeação e exclusão. Progresso, bytes, arquivos, velocidade/estimativa quando disponíveis. |
| Seleção múltipla — implementado | `toggleMark`, `selectionMenu`, `Pane::marks`. Marcar todos visíveis, arquivos, pastas, inverter e limpar. As marcações de tipos são aditivas e respeitam o filtro; reload limpa as marcações. |
| Organização/localização — implementado | `Pane::sortItems`, `applyFilter`, `sortMenu`, `favMenu`, `favoritesList`, `historyList`, `historyAdd`. Nome/tamanho/data/tipo, inversão, filtro local, atualização, favoritos e até 15 caminhos recentes salvos. Estado de ordenação/filtro é de sessão. |
| Fila — implementado | `QueuedTransfer`, `enqueueTransfer`, `runTransferQueue`, `transferQueueMenu`, `QUEUE_LIMIT`. Até 20 tarefas, somente SD, copiar/mover, listar/executar/limpar e progresso. Em memória, sem persistência e sem transação de toda a tarefa. |
| Google Drive — experimental | [gdrive.cpp](../src/cloud/gdrive.cpp): `loadCreds`, `tokenRequest`, `refreshToken`, `loginServerStart/Poll/Stop`, `listFolder`, `downloadFile`, `uploadFile`, `createFolder`, `renameItem`, `trashItem`, `logout`; main: `ensureDriveLogin`, `driveLoad`, `scanDriveTree`, `dlTree`, `ulTree`, `toggleDrive`, `cloudMenu`, `logoutAll`. Conta individual, OAuth manual, navegação/paginação, árvores SD↔Drive, pastas/nomes/lixeira, renovação e desconexão. Falha física de persistência do login relatada, não resolvida aqui. |
| FTP — implementado | [wifi_transfer.cpp](../src/network/wifi_transfer.cpp): `start`, `serve`, `session`, transferências e contadores; main: `wifiMenu`, `modal`, `drawMain`. Porta 2121, quatro clientes no código, leitura/escrita anônima, ativo/passivo, REST de download, eventos e refresh local. |
| ZIP — implementado | [zipx.cpp](../src/archives/zipx.cpp): `inspect`, `compress`, `extract`; main: `zipMenu`, `showZipInfo`, `doCompressZip`, `doExtractZip`. Seleção local, modos rápido/sem compressão, destino atual/oposto, estatísticas, senha detectada/itens ignorados, CRC e cancelamento. |
| Armazenamento/propriedades — implementado | `refreshSpace`, `propsText`, `doProperties`, `calcTree`, `doSpaceAnalyzer` em main. Total/livre/utilização SD, propriedades individuais e agregadas, arquivos/pastas, data quando disponível, até nove maiores itens imediatos. Totais podem ser incompletos; profundidade limitada no cálculo de propriedades. |
| Homebrew NRO — implementado | `endsWithNro`, `openCur`, `launchNro`, `envHasNextLoad`, `envSetNextLoad`. Extensão NRO local, confirmação e saída para o programa quando suportado. Outros arquivos mostram informações, sem visualizador universal ou instalador de jogos. |
| Proteções — implementado | [path_policy.cpp](../src/security/path_policy.cpp), `doCopyMove`, `doDelete`, [operations.cpp](../src/storage/operations.cpp), verificações do FTP/ZIP. Confirmações, nomes/caminhos, destinos exclusivos nos fluxos protegidos, conferência do fallback e erros explícitos; não há garantia absoluta ou transação global. |
| Diagnósticos — implementado | [logger.cpp](../src/diagnostics/logger.cpp), chamadas `sfm_diag::write` no app/FTP/renderer. Início/fim, menu Ações e eventos/erros instrumentados, timestamp, mutex, rotação próxima de 1 MiB, retorno de falha e stderr. Não há tela própria de diagnóstico nem cobertura de todo erro de todos os módulos. |

As funções da aplicação mencionadas estão em [src/app/main.cpp](../src/app/main.cpp). Os guias de [controles](CONTROLES.md), [armazenamento](ARMAZENAMENTO.md), [Drive](GOOGLE_DRIVE.md), [FTP](FTP.md) e [ZIP](ZIP.md) detalham o comportamento e suas restrições.

## Mapa de todos os menus principais

Os rótulos abaixo são os textos presentes na revisão inventariada, conservados para evitar omitir ferramentas ao escrever a apresentação.

### Ações

| Opção | Onde está documentada |
| --- | --- |
| Copiar para o outro painel | Arquivos/pastas e armazenamento. |
| Mover para o outro painel | Arquivos/pastas, bloqueio SD↔Drive e fallback local. |
| Renomear | Arquivos/pastas e Drive. |
| Nova pasta | Arquivos/pastas e Drive. |
| Apagar | Arquivos/pastas, confirmação e lixeira Drive. |
| Seleção múltipla... | Menu de marcação abaixo. |
| Fila de transferências... | Fila local e tarefas pendentes. |
| Ordenar e buscar... | Ordenação, filtro e atualização. |
| Compactar e extrair (ZIP)... | ZIP local. |
| Propriedades e espaço... | Propriedades e maiores itens. |
| Favoritos e histórico... | Navegação persistida em caminhos legados. |
| Google Drive e conta... | Painel Drive, retorno ao SD e troca de conta. |
| Transferência Wi-Fi... | Inicialização/estado/parada do FTP. |

### Seleção múltipla

- Marcar todos os itens visíveis.
- Marcar somente arquivos.
- Marcar somente pastas.
- Inverter marcação visível.
- Limpar todas as marcações.

### Ordenar e buscar

- Ordenar por nome.
- Ordenar por tamanho.
- Ordenar por data.
- Ordenar por tipo.
- Inverter a ordem.
- Buscar por nome...
- Limpar a busca.
- Atualizar as listas.

### Fila de transferências

- Adicionar cópia do painel ativo.
- Adicionar movimentação do painel ativo.
- Executar fila.
- Ver tarefas pendentes.
- Limpar fila (sem apagar arquivos).

### ZIP

- Compactar a seleção (rápido).
- Compactar a seleção (sem compressão).
- Extrair o ZIP selecionado aqui.
- Extrair o ZIP selecionado no outro painel.
- Informações do ZIP.

Abrir um ZIP também oferece Cancelar, Extrair aqui, Extrair no outro painel e Informações. O aviso de ZIP com senha permite cancelar ou continuar com itens que forem suportados.

### Propriedades e espaço

- Propriedades do item (ou da seleção).
- Espaço ocupado (maiores itens da pasta).

### Favoritos e histórico

- Adicionar esta pasta aos favoritos.
- Ver favoritos (com quantidade atual).
- Pastas recentes (com quantidade atual).
- Em um favorito: Ir para a pasta, Remover dos favoritos, Cancelar.
- No histórico: caminhos recentes e Limpar o histórico, com confirmação.

### Google Drive e conta

- Abrir o Google Drive neste painel **ou** Voltar este painel ao cartão SD, conforme estado.
- Trocar de conta do Google, com confirmação Desconectar/Cancelar.
- Diálogo de login: endereço local para o navegador do celular e Cancelar por B/toque.

Voltar o painel ao SD conserva o login em memória. A desconexão confirmada chama `gd::logout`, remove o token salvo desse fluxo e volta os painéis Drive ao SD; não apaga a credencial `google.json`. A Missão 2 não executa essa ação sobre dados reais.

### Transferência Wi-Fi e ações por contexto

- Ao ativar FTP: Cancelar ou Iniciar FTP, após o aviso de acesso total.
- Servidor ativo: Manter ativo ou Parar servidor FTP; atividade atualizada no modal.
- NRO local: Cancelar ou Abrir, com aviso de encerramento do gerenciador.
- Arquivo comum: tamanho e pasta; documento nativo Google recebe aviso de impossibilidade de download/exportação.
- Confirmações de exclusão local, lixeira remota, pasta crítica e limpeza de histórico/fila são contextuais, não funcionalidades transacionais.

## Recursos adicionais e limites conferidos

- O indicador superior mostra marcações, filtro e atividade FTP conforme estado.
- O progresso calcula velocidade média e ETA somente quando há dados/tempo suficientes; a contagem por arquivo tem particularidades, por exemplo rename local instantâneo.
- Nomes disponíveis evitam sobrescrever nos fluxos da aplicação; isso não equivale a uma trava global entre FTP e operações locais.
- Propriedades/análise permitem cancelamento; exclusão recursiva não oferece parada imediata garantida ao ler B.
- Não há tela Sobre, versão no cabeçalho principal, indicador de bateria, relógio, visualizador geral, atualização automática, USB externo ou instalação de NSP/XCI/NSZ implementados.
- [Roadmap](ROADMAP.md) registra recursos futuros. Ícones da marca não servem de evidência funcional.

## Evidências de teste

A suíte de host testa módulos locais, logs, ZIP real, FTP em loopback e Drive simulado. A aplicação é compilada com devkitPro e o NRO é validado estruturalmente. **Menus, toque, controles, desempenho no console e OAuth/SSL reais não se tornam aprovados por esses resultados.** O relato de falha física de persistência é evidência de problema, não de aprovação da matriz do Switch. Resultados novos desta missão ficam no [relatório de validação](RELATORIO_VALIDACAO.md).
