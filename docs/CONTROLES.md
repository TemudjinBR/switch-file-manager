# Controles e navegação

Estado: **implementado no código**, validação física integral da revisão pendente. Fonte principal: [main.cpp](../src/app/main.cpp), funções `handleInput`, `modal`, `updateTouch`, `toggleMark`, `tickProgress`, `ensureDriveLogin` e `askText`.

## Listas e tela principal

| Entrada | Ação |
| --- | --- |
| Direcional ↑/↓ ou analógico esquerdo ↑/↓ | Move o foco; após o atraso inicial, manter pressionado repete. |
| Direcional ←/→ | Ativa respectivamente painel esquerdo/direito. Não é um atalho de subir/entrar em pasta. |
| L ou R | Alterna o painel ativo. |
| ZL/ZR | Move o foco uma página acima/abaixo, conforme a quantidade de linhas da interface. |
| A | Abre pasta; em ZIP abre as opções; em NRO pede confirmação; em outros arquivos mostra informações básicas. |
| B | Sobe uma pasta do painel ativo. Na raiz local ou do Drive, permanece nela. |
| Y | Marca/desmarca o item e avança o foco, se houver próximo item. |
| ZL + Y | Abre as opções avançadas de marcação (todos, somente arquivos, somente pastas, inverter, limpar). |
| ZL + X | Abre as propriedades detalhadas do item ou da marcação múltipla. |
| X | Abre o menu Ações: categorias gerais sem seleção, operações diretas com item selecionado. |
| − | Alterna o painel ativo entre SD e Drive; pode abrir o fluxo de login. Voltar ao SD não equivale a desconectar a conta. |
| + | Solicita saída na tela principal. |

O rodapé oferece as mesmas ações por toque: **Abrir**, **Voltar**, **Marcar**, **Ações**, **Painel**, **Nuvem** e **Sair**.

## Menus e modais

- ↑/↓ ou analógico esquerdo vertical mudam a opção; nessa lógica a seleção pode circular entre primeira e última.
- ZL/ZR saltam a quantidade de opções visíveis.
- A confirma; B cancela.
- Tocar em uma opção a aciona; tocar fora da caixa cancela esse menu.
- Arrastar a lista rola quando existem mais opções do que a área comporta.

**+ não é tratado como saída global dentro de qualquer modal.** Os contextos de login e progresso têm lógica própria. Não há atalho de teclado externo anunciado.

## Toque nas listas

O primeiro toque seleciona e ativa o painel. Tocar no mesmo item já em foco no painel ativo o abre; não há um temporizador de “duplo clique” específico. Arrastar verticalmente rola o painel iniciado pelo gesto. Tocar no cabeçalho também pode selecionar o painel.

Rolagem por toque não é a mesma coisa que mover o foco com os botões. Por isso, um item previamente focado pode ficar fora da parte rolada até nova navegação/seleção.

## Progresso e login

`tickProgress` lê B para solicitar cancelamento nas operações que o consultam. Isso inclui cópias, comparações e pontes de progresso do Drive/ZIP e cálculos locais, com diferenças por operação. Não significa que toda etapa ou remoção seja interrompida imediatamente, nem desfaz ações já concluídas.

No login Drive, B ou o botão Cancelar por toque encerra o diálogo local; ele não é uma correção da falha de salvar token. Para nomes, renomeações e busca, o aplicativo usa o teclado do sistema (`swkbd`), que depende do ambiente de execução.

## Menu contextual — patch experimental 0.10.13

**Sem item escolhido:** ao abrir o programa/entrar na pasta, X apresenta **Arquivos e pastas**, **Transferências**, **Organização** e **Sistema**. Não há categoria Ferramentas.

**Com um arquivo/pasta selecionado:** X apresenta **Copiar, Mover, Selecionar tudo, Excluir, Renomear** e **Compactar em ZIP**. Se o item local é ZIP, aparece **Extrair ZIP** em vez de Compactar.

**Com vários itens marcados:** X apresenta **Copiar, Mover, Desmarcar tudo, Excluir** e **Compactar em ZIP**; quando todos são ZIPs locais, a última ação é **Extrair ZIP**, aplicável a todos, com destino escolhido e pastas únicas.

Ao mover o cursor com controles ou tocar num item, esse item se torna o alvo contextual. **O primeiro destaque automático após carregar uma pasta não equivale a selecionar.** Ao recarregar, marcações e foco contextual são limpos. Alterar de painel preserva o foco próprio de cada painel. Copiar/mover usam o painel oposto como destino. As ações de ZIP só aparecem em conteúdos locais, não no Drive.

**Sistema → Sobre:** informações de versão e autoria, depois seção de contribuição voluntária com QR Pix sem valor fixo. Nenhuma referência ao Pix aparece no menu Sistema.

**Transferências:** reúne links para fila local, servidor FTP, conta Drive e downloads recuperáveis. Esta primeira reorganização **não unifica os mecanismos internos numa única fila**; não atribuir a esta revisão essa capacidade.

Consulte também [o novo mapa contextual](MENU_CONTEXTUAL.md).

## Esperas Drive — candidata 0.10.12

Durante navegação/transferência remota, B solicita cancelamento e Y inicia/para o FTP; a primeira ativação pede confirmação do acesso anônimo. Fora dessa espera, Y continua marcando itens como antes. X e toque continuam Pausar/Continuar em downloads, B/toque Cancelar. DNS e E/S SD podem atrasar a resposta. Não há navegação livre dos painéis durante uma transferência Drive síncrona.

## Atualizador — Missão 3.3

X → Sistema → Atualizações usa o modal existente: botões/toque para verificar, notas, baixar, instalar, recuperar anterior, alternar canal/checagem e cancelar tarefa. B sai do diálogo sem cancelar implicitamente o worker. Instalação/rollback exigem confirmação; o auxiliar mostra erro em console e B para sair. [Detalhes](ATUALIZACOES.md).
