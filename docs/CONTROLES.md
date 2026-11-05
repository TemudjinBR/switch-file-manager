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
| X | Abre o menu Ações. |
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

## Caminho para as ferramentas

**X → Ações** reúne copiar, mover, renomear, criar pasta, apagar e os submenus Seleção múltipla, Fila de transferências, Ordenar e buscar, ZIP, Propriedades e espaço, Favoritos e histórico, Google Drive e Transferência Wi-Fi. Consulte o [mapa completo dos menus](FUNCIONALIDADES.md).
