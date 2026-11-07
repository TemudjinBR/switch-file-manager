# Menu global e contextual — Missão 3.6

Candidata **0.10.14 experimental**, derivada do PR #2 (`02d6b065bb20a888452d7f380fb96f84f2abb402`), que depende do PR #1 (`84e3f769e69380bfae74f7a23a0e3c67f7a9188e`). Ambos preservados. **PENDENTE DE TESTE FÍSICO**.

## Matriz de entrada

| Contexto nos dois painéis | + Menu | X Ações |
| --- | --- | --- |
| SD ou Drive, pasta vazia, sem foco/marcações | Cinco categorias globais | Selecione um arquivo ou pasta |
| Um arquivo/pasta local | Cinco categorias globais | Copiar, Mover, Selecionar tudo, Excluir, Renomear, Compactar em ZIP |
| Um ZIP local | Cinco categorias globais | Mesmas ações, com Extrair ZIP no lugar de Compactar em ZIP |
| Vários arquivos/pastas locais | Cinco categorias globais | Copiar, Mover, Desmarcar tudo, Excluir, Compactar em ZIP |
| Vários ZIPs locais | Cinco categorias globais | Copiar, Mover, Desmarcar tudo, Excluir, Extrair ZIP (todos os selecionados) |
| Um item Drive | Cinco categorias globais | Copiar, Selecionar tudo, Excluir, Renomear |
| Vários itens Drive | Cinco categorias globais | Copiar, Desmarcar tudo, Excluir |

Um destaque inicial automático não equivale a foco contextual. Mover o cursor, tocar ou marcar atribui foco. Marcações têm prioridade sobre o cursor; abrir/fechar menus não as altera. Recarga por modificação externa invalida a seleção e cancela o contextual se necessário.

## Inventário antes/depois

| Antes: patch sobre 0.10.13 | Agora: 0.10.14 |
| --- | --- |
| + encerrava; − abria nuvem | + Menu global; − Sair com confirmação |
| X sem alvo abria quatro categorias | X sem alvo apenas orienta; menu independente em + |
| Raiz: Arquivos e pastas, Transferências, Organização, Sistema | Raiz: Arquivos e pastas, Transferências, Organização, Sistema, Sobre |
| Sistema: Atualizações, Configurações do atualizador, Versão e diagnósticos, Sobre | Sistema: Atualizações, Versão e diagnósticos |
| Configurações e Atualizações abriam a mesma tela | Duplicação removida; configurações reais continuam dentro de Atualizações |
| Alternar SD/Drive dependia de − ou menu da conta | Arquivos e pastas oferece alternância explícita; menu da conta preservado |
| Drive oferecia Mover, rejeitado na execução | Mover remoto não é oferecido; cópia permanece |
| Sobre usava 48×48 com paleta ampliada | Arte oficial RGB888 256×256 nativa, sem quantização |

- **Arquivos e pastas:** Nova pasta, Espaço total e livre do microSD, Analisar espaço desta pasta, Caminho do painel ativo, Alternar microSD / Google Drive neste painel.
- **Transferências:** Fila local de cópias e movimentos, Servidor FTP (Wi-Fi), Google Drive e conta, Downloads do Google Drive. São acessos reais; filas continuam independentes.
- **Organização:** Ordenar e pesquisar, Favoritos e histórico, Atualizar painéis.
- **Sistema:** Atualizações, Versão e diagnósticos (caminho em execução e busca somente leitura de NROs).
- **Sobre:** nome, descrição, autor, versão/build incorporados, canal de atualizações, URL; abaixo, apoio opcional com Pix original. Não há Pix/Doação/Apoio na raiz ou Sistema.

## Preservação e limites

A/B/Y/L/R, direção, toque, ZL+Y e ZL+X são mantidos. ZIP usa os motores existentes; múltiplos ZIPs têm pastas exclusivas por arquivo e validação de caminhos/CRC. Excluir mantém confirmação. FTP fica na porta 2121. Credenciais, manter conectado, desconectar/reconectar e dados legados não são modificados por esta missão.

− pede confirmação, sinaliza cancelamento cooperativo e encerra os serviços na saída normal. Recusar não altera tarefas/seleção. A fila local não persiste entre execuções; downloads Drive preservam recuperação existente, sem alegar retomada de upload. + durante operação síncrona aguarda retorno seguro. Teclado do sistema e auxiliar transacional têm regras próprias. [Controles](CONTROLES.md).

## Checklist no Switch Lite

**Todos os itens abaixo: PENDENTE DE TESTE FÍSICO.** Instalar conforme [guia Android](VERSAO_E_INSTALACAO.md).

1. Conferir 0.10.14, build do manifesto, caminho NRO e rodapé + Menu / X Ações / − Sair. Se divergirem, diagnosticar cópias antes de qualquer remoção.
2. **+** abre as cinco categorias, na ordem correta, nos dois painéis, SD/Drive, pasta vazia, zero/um/vários marcados. B e + preservam marcações ao retornar.
3. X sem foco somente orienta; com arquivo, pasta, ZIP e múltiplos apresenta a matriz acima. Renomear nunca aparece para múltiplos; Drive não oferece Mover/ZIP.
4. A/B/Y/L/R, ZL/ZR, ZL+Y, ZL+X, direção e toque: testar seleção, abertura e limites dos botões inferiores.
5. SD↔Drive pela nova rota; repetir login, manter conectado, desconectar/reconectar, navegação/cópia fictícia. Não apagar credenciais.
6. Abrir fila local, FTP isolado/antes/depois do Drive e downloads recuperáveis; manter limitações físicas anteriores registradas.
7. − ocioso e durante cópia fictícia, ZIP, login, FTP com cliente e worker de atualização: primeiro recusar, depois confirmar; conferir sincronização, arquivos pessoais intactos e reabertura. Não interromper instalação do auxiliar.
8. **+** durante download: pedido aguarda retorno seguro; pausar/continuar/cancelar existentes funcionam. Queda de rede/retomada continuam exigindo validação física própria.
9. Sobre na quinta entrada, ausente de Sistema; comparar ícone com arte oficial, proporção e texto; conferir canal de atualizações e build reais, retorno por botão/toque.
10. Escanear QR com outro aparelho **sem confirmar pagamento**: nome original, valor livre, quiet zone legível; apoio opcional abaixo das informações do app.
11. Conferir duplicados inclusive ocultos em modo somente leitura, NACP/build/SHA-256; nenhum NRO/configuração deve ser apagado automaticamente.

## Evidências reproduzíveis

- [Política de entrada](../include/ui/navigation.h), [contexto](../include/ui/context_actions.h), [integração](../src/app/main.cpp).
- [Desenho do Sobre](../src/ui/about_view.cpp), [renderer](../src/ui/renderer.cpp), [gerador RGB](../scripts/generate_about_icon.py).
- [Arte original](../assets/icon.jpg), [QR original](../assets/pix_doacoes.svg), [matriz embarcada](../include/ui/pix_qr.h).
- [Testes executáveis](../tests/test_navigation.py), [contexto](../tests/test_context_menu.py), [relatório e prévia host](MISSAO_3_6.md).
