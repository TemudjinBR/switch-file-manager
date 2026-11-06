# Menu contextual — patch experimental sobre 0.10.13

## Objetivo

Reduzir o número de opções no botão X sem perder funções implementadas.
A implementação ocorre em branch própria, derivada do SHA base
`84e3f769e69380bfae74f7a23a0e3c67f7a9188e`. Não substitui o NRO
0.10.13 validado antes; precisa de nova compilação e homologação no Lite.

## Matriz de apresentação

| Situação no painel ativo | Botão X |
| --- | --- |
| Sem foco atribuído pelo usuário e sem marcações | Arquivos e pastas, Transferências, Organização, Sistema |
| Um arquivo comum (microSD) | Copiar, Mover, Selecionar tudo, Excluir, Renomear, Compactar em ZIP |
| Uma pasta (microSD) | Copiar, Mover, Selecionar tudo, Excluir, Renomear, Compactar em ZIP |
| Um arquivo ZIP (microSD) | Copiar, Mover, Selecionar tudo, Excluir, Renomear, Extrair ZIP |
| Vários arquivos/pastas marcados (microSD) | Copiar, Mover, Desmarcar tudo, Excluir, Compactar em ZIP |
| Vários ZIPs marcados (microSD) | Copiar, Mover, Desmarcar tudo, Excluir, Extrair ZIP |
| Item(s) no Drive | Copiar, Mover, Selecionar/Desmarcar tudo, Excluir, Renomear só para item único; **ZIP indisponível** |

**Foco não é marcação.** Ao entrar numa pasta, o cursor fica na primeira
linha para navegação, mas o botão X permanece geral até o usuário mover o
foco/tocar num item ou usar Y. Y continua marcando/desmarcando itens
individualmente. O atalho ZL+Y mantém marcar apenas arquivos, apenas
pastas, inverter, limpar. ZL+X abre propriedades detalhadas.

- **Arquivos e pastas:** criar pasta, ver total/livre do SD, analisar espaço
  da pasta atual, visualizar o caminho.
- **Transferências:** estado resumido, fila **local**, servidor FTP e seu
  diagnóstico, Google Drive/conta, downloads recuperáveis do Drive.
- **Organização:** ordenar/pesquisar, favoritos/histórico, atualizar listas.
- **Sistema:** atualizações, configurações do atualizador, versão/diagnósticos,
  Sobre. Nenhum item menciona Pix ou doação.
- **Sobre:** explicação, autoria, versão/build derivados de macros oficiais,
  endereço GitHub e abaixo convite discreto de contribuição voluntária com
  QR Pix estático **offline**, sem valor fixo.

## ZIP, segurança e preservação

Compactar reúne um ou vários itens, incluindo pastas inteiras, em um
único ZIP. O modo rápido e sem compressão existente é preservado.
Extrair um ou vários ZIPs cria pasta de saída independente por ZIP,
preserva destino selecionado (painel atual ou oposto) e passa pela
validação de caminhos/CRC do extrator existente. Não oferece extração
ZIP de arquivos no Drive sem baixá-los. O comando de excluir continua
exigindo confirmação e aviso em pastas críticas; mover do/para Drive
continua bloqueado pela proteção já existente.

O menu reage ao painel ativo. Recarregamentos limpam seleção e
contexto; caso a lista seja alterada durante a janela de ações, o
comando não deve operar sobre um item diferente por acidente.
Operações físicas exigem teste no Switch Lite; testes de host não
comprovam toque, foco, layout, imagem QR escaneável ou memória final.

## Limites conhecidos mantidos

- Este patch **não faz verdadeira unificação das filas** local, FTP e Drive.
  O submenu Transferências é um ponto de acesso e resumo; as operações
  continuam com mecanismos próprios, inclusive fila local de 20 tarefas.
- Não modifica credenciais nem OAuth, não lança releases, não faz merge e
  não habilita pagamento dentro do aplicativo.
- O QR apenas mostra dados de Pix para transferência feita pelo usuário
  no próprio banco. A doação é opcional e não desbloqueia recursos.
- O usuário deve conferir o destinatário antes de pagar.

## Homologação necessária no Lite

1. Iniciar sem marcar e confirmar X com quatro categorias, nos dois painéis.
2. Destacar arquivo/pasta por controle e toque e testar todas as ações,
   inclusive após mudar de painel ou entrar/sair de pastas.
3. Marcar e desmarcar itens por Y; selecionar tudo; usar ZL+Y e ZL+X.
4. Testar compacto de pasta, de vários tipos mistos e de vários ZIPs;
   extração de ZIP único e múltiplos com conflitos de destino, cancelamento
   e falta de espaço; verificar arquivos de origem intactos.
5. Testar Drive selecionado e confirmar ausência de ZIP local, preservando
   cópia e bloqueio de movimento remoto.
6. Sistema → Sobre, conferir versão/build real e escanear QR Pix num
   segundo aparelho SEM confirmar pagamento; verificar valor livre e
   beneficiário correto. Verificar legibilidade e botão de retorno.
7. Confirmar acesso à fila local, FTP e Google Drive no submenu Transferências
   e regressão com transferências simultâneas/pendentes.
8. Executar testes host, compilação real do principal e auxiliar,
   validação de NACP/ícone oficial e checksums antes de instalar no Lite.

## Evidências

- Modelo puro: [include/ui/context_actions.h](../include/ui/context_actions.h).
- Integração de tela: [src/app/main.cpp](../src/app/main.cpp).
- QR embarcado: [include/ui/pix_qr.h](../include/ui/pix_qr.h).
- QR do README: [assets/pix_doacoes.svg](../assets/pix_doacoes.svg).
- Testes: [tests/test_context_menu.py](../tests/test_context_menu.py).

Estado de compilação/QA: consultar o Actions do PR específico do patch;
não declarar aprovado sem os resultados daquele SHA.
