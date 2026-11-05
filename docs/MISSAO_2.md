# Missão 2 — identidade, apresentação e organização do GitHub

Base: `8be971079763e6c74bb3b6f37f1e25d3b976e35f`, branch `codex/reorganizacao-estabilizacao`, PR #1. Escopo: README, identidade oficial, inventário, guias, organização documental e verificações correspondentes. Não altera código C++, versão, bibliotecas incorporadas, ícone do executável ou dados de usuário.

## Identidade visual

O logotipo de `assets/icon.jpg` corresponde à descrição oficial do proprietário. Foi reutilizado diretamente no topo do README, centralizado, com 192×192 de apresentação e texto alternativo. Os bytes do JPEG 256×256 permanecem os mesmos, incluindo nome, símbolos, cores e composição. Não foram criados banner, nova marca, mockups ou capturas fictícias do aplicativo. Foram adicionadas somente duas imagens de evidência da própria documentação renderizada, `docs/validacao/missao2/readme-desktop.png` e `readme-mobile.png`, que não são telas do Switch.

Não foram encontradas screenshots autênticas no repositório, ZIP legado ou anexos desta missão. `docs/screenshots/README.md` prepara a documentação para recebê-las sem links quebrados. As regras de preservação estão em `assets/branding/README.md`.

## README e inventário

O README apresenta introdução, oito benefícios, catálogo completo de 12 categorias, estado experimental, instalação pública/teste, controles por contexto, integrações, mapa de arquitetura, acompanhamento e créditos/licenciamento.

As 12 categorias são: interface/navegação, arquivos/pastas, seleção múltipla, organização/localização, fila local, Drive, FTP, ZIP, armazenamento/propriedades, homebrew NRO, proteções e diagnósticos. Cada menu/submenu está mapeado em `docs/FUNCIONALIDADES.md` e os detalhes de persistência e limites nos guias específicos.

## Organização dos arquivos

- Criados: índice `docs/README.md`, inventário `docs/FUNCIONALIDADES.md`, guias `CONTROLES.md`, `ARMAZENAMENTO.md`, `FTP.md`, `ZIP.md`, este relatório, regras de marca/capturas, verificador `scripts/check_docs.py`, testes de documentação, configuração `.markdownlint-cli2.jsonc` e duas prévias do README em `docs/validacao/missao2/`.
- Movido: relatório anterior para `docs/historico/VALIDACAO_MISSAO_1.md`, preservando evidências e corrigindo links relativos.
- Atualizados: README, CHANGELOG, referência, arquitetura, testes, Drive, roadmap, problemas conhecidos, auditoria, relatório atual e notas 0.10.8.
- Removidos: nenhum material histórico, recurso funcional ou arquivo de compatibilidade. Os seis wrappers de `scripts/legacy` permanecem verificados. A arquitetura anterior e a raiz enxuta foram preservadas.

## Campo Sobre e tópicos — pendentes por limitação externa

Estado observado no GitHub em 05/11/2026:

- Descrição atual: `Gerenciador de arquivos homebrew para Nintendo Switch, em C++17.`
- Tópicos atuais: nenhum.
- Homepage atual: vazia; não existe página própria publicada a configurar.

Descrição proposta:

> Gerenciador de arquivos para Nintendo Switch desbloqueado: dois painéis, operações em lote no SD, fila local, FTP, ZIP e Google Drive experimental.

Tópicos propostos:

```text
nintendo-switch, switch-homebrew, file-manager, atmosphere, libnx,
devkitpro, cpp, ftp, google-drive, zip
```

**Não aplicados.** O conector disponível não expõe mutação desses metadados. O acesso normal pela CLI à API (`gh api repos/TemudjinBR/switch-file-manager`) retornou `Forbidden`; o executor tem política HTTP restrita e não libera o host da API em sua lista. O erro ocorreu no acesso, antes de uma alteração administrativa confirmada. Não é evidência de que o proprietário deixou de ter permissão no repositório.

Não foram solicitados tokens, alteradas credenciais ou usados caminhos alternativos para contornar a restrição. README não é o campo Sobre. A visibilidade do repositório permanece pública. Esse critério da missão fica **não concluído** no ambiente disponível.

## Testes e validação

Os resultados novos de verificador, suíte, build, NRO, renderização e CI ficam em [RELATORIO_VALIDACAO.md](RELATORIO_VALIDACAO.md). A validação anterior é histórica e não substitui a execução atual. A versão permanece 0.10.8 experimental.

## Pendências técnicas preservadas

O relato físico “Falha ao salvar login no cartão SD.” no Switch Lite foi documentado; a causa exata da persistência não está confirmada e o fluxo OAuth não foi alterado. Fila permanece local/não persistente, Drive experimental, FTP anônimo/sem TLS, limites de ZIP/SD e validações físicas continuam declarados.

USB OTG, atualização automática, instalador de pacotes de jogos e novos provedores continuam planejados/indisponíveis. O [roadmap](ROADMAP.md) e os [problemas conhecidos](PROBLEMAS_CONHECIDOS.md) detalham esses estados.

O trabalho segue no [PR #1](https://github.com/TemudjinBR/switch-file-manager/pull/1), sem merge, force-push, edição direta da main ou publicação de Release. A apresentação nova está na branch do PR; a página principal da main só mudará após integração autorizada.
