# Controles e navegação — 0.10.14 experimental

Implementados no código; **PENDENTE DE TESTE FÍSICO no Switch Lite**. A política compartilhada está em [navigation.h](../include/ui/navigation.h), integrada por `handleInput`, `dispatch` e pelo rodapé em [main.cpp](../src/app/main.cpp).

## Tela principal

| Entrada | Ação |
| --- | --- |
| + | Menu global, sempre: Arquivos e pastas, Transferências, Organização, Sistema, Sobre. Não altera seleção. |
| X | Somente ações contextuais. Sem foco/marcações, orientação “Selecione um arquivo ou pasta”. |
| − | Confirmação de saída; recusar preserva o trabalho; confirmar solicita cancelamento e aguarda os serviços. |
| A / B | Abrir item / subir à pasta superior. Na raiz, B permanece nela. |
| Y | Marcar/desmarcar item e avançar foco, quando possível. |
| ZL + Y | Seleção avançada: todos, arquivos, pastas, inverter, limpar. |
| ZL + X | Propriedades detalhadas do item ou da seleção. |
| L ou R | Alternar painel ativo, preservando o contexto próprio de cada painel. |
| ← / → | Ativar painel esquerdo / direito. |
| ↑/↓ ou analógico esquerdo vertical | Mover foco; manter pressionado repete após atraso. |
| ZL / ZR isolados | Página acima / abaixo. |

Rodapé tátil: **A Abrir, B Voltar, Y Marcar, X Ações, L/R Painel, + Menu, − Sair**. Usa a mesma política dos comandos físicos. Para SD↔Drive: **+ → Arquivos e pastas → Alternar microSD / Google Drive neste painel**. Transferências → Google Drive e conta também oferece essa alternância.

## Menus, Sobre e toque

- ↑/↓ ou analógico vertical navegam; ZL/ZR saltam opções; A confirma; B cancela.
- **+** fecha a cadeia de modais e solicita o menu global; nunca equivale a sair.
- − abre confirmação de saída; B ou Continuar no aplicativo a recusa. Durante essa confirmação, +/− não substituem a escolha explícita.
- Sobre: A/B/X ou botão Voltar retornam; + retorna ao menu global; − usa a mesma saída segura.
- Toque em opção executa; fora do modal cancela. Arrastar rola listas longas.
- Primeiro toque atribui foco/ativa painel; tocar novamente no mesmo item focado abre. Arrastar a lista não altera automaticamente o foco. Não há temporizador de duplo clique.

Abrir/fechar o menu não limpa marcações. Recarregar a pasta, ou detectar alterações FTP que exigem recarga, pode invalidá-las; o contextual cancela se seu alvo mudou.

## Operações em andamento

B solicita cancelamento nas operações que o consultam; Y inicia/para FTP durante esperas Drive, com confirmação inicial. Download Drive mantém X/toque Pausar/Continuar e B/toque Cancelar.

Durante cópia, ZIP ou requisição Drive síncrona, + registra o pedido e abre o menu quando a operação devolve controle, sem interromper uma gravação. − confirma, sinaliza cancelamento cooperativo e espera o retorno; depois encerra o worker de atualizações, FTP e sockets. Etapas já concluídas não são desfeitas. E/S SD, DNS e operações que não consultam progresso podem atrasar a resposta; não se promete saída instantânea.

No login por navegador, B/toque cancelam; + cancela apenas esse diálogo e pede menu; − confirma saída e encerra o servidor local. A conta anterior é preservada pelo fluxo OAuth existente. O teclado do sistema e o Homebrew Menu são interfaces externas ao aplicativo: seus atalhos próprios não são remapeados. A transação do auxiliar não recebe atalho para interrupção no meio da instalação.

A fila local é somente em memória e a saída avisa que tarefas ainda não executadas não serão salvas. Downloads recuperáveis usam seus checkpoints existentes. Isso não transforma FTP/Drive/fila local em um motor unificado. [Matriz contextual e checklist](MENU_CONTEXTUAL.md).
