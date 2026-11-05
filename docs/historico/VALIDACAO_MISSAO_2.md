# Validação da revisão — Missão 2 (05/11/2026)

## Resultado desta execução

**45 testes passaram com o NRO da nova compilação, sem testes ignorados nessa execução.** A versão permanece **0.10.8 experimental**, com `SwitchFileManager.nro` de **779.992 bytes**. O ícone incorporado foi comparado byte a byte com a marca oficial e permaneceu intacto.

Esta é uma execução nova, feita sobre a apresentação reformulada. Os resultados da Missão 1 foram preservados em [histórico](VALIDACAO_MISSAO_1.md). O C++, headers, Makefile, versão, ícone original, bibliotecas incorporadas e workflows não foram alterados pela Missão 2. Os fontes do aplicativo continuam os da base `8be971079763e6c74bb3b6f37f1e25d3b976e35f` do PR #1.

| Verificação nova | Resultado observado |
| --- | --- |
| `python3 scripts/check_docs.py` | 22 documentos: links locais Markdown/HTML, âncoras e referências a imagens válidos. |
| `python3 tests/run.py` | 45 casos reconhecidos; 43 aprovados e dois testes de NRO ignorados explicitamente por não receber binário nessa etapa. |
| `bash scripts/build.sh` | Testes no host, compilação devkitPro pelo ambiente preparado, validação NRO, testes com binário e pacote concluídos. |
| `python3 tests/run.py --nro SwitchFileManager.nro` durante o build | 45/45 aprovados, sem ignorados; 18,016 s nessa execução. |
| NRO | NRO0, segmentos, ASET, limites de assets, JPEG, NACP, título e versão 0.10.8 aprovados. |
| Ícone oficial | Hash do JPEG original preservado; teste conferiu os mesmos bytes dentro do NRO. |
| Markdownlint | CLI 0.18.1 / engine 0.38.0: 21 arquivos, zero erros, com configuração explícita do repositório. |
| Workflows | `actionlint` 1.7.7 passou; os arquivos de workflow desta missão permanecem inalterados. |
| Prévia renderizada | Chromium com MarkdownIt/tabelas e github-markdown-css 5.8.1; quatro tamanhos, sem overflow da página e sem falha de carregamento do logotipo. |
| Links externos principais | Sete URLs GitHub do README/guias retornaram HTTP 200: perfil do autor, Actions, Issues, PR #1, Releases, v0.10.7 e Omni36OS. |
| Links Google externos | Console Cloud e documentação OAuth não verificados diretamente: domínios fora da política HTTP do executor. Não foram declarados aprovados por inferência. |
| `git diff --check` | Aprovado. |
| Segurança documental | Sem valores reais de tokens, client secrets, códigos OAuth ou dados pessoais adicionados; exemplos/testes usam dados fictícios. |

## Compilação e artefatos

Imagem do ambiente:

```text
devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282
```

A nova compilação usou `/workspace/sfm-environment/build.sh`, via script do repositório, em cópia descartável. Resultado local:

```text
SwitchFileManager.nro — 779992 bytes, versão NACP 0.10.8
SHA-256: 9aa5dabdd87a426540b458fd78d36505ba9a51b9c2f3b3e18eb7d0ee012350cc

assets/icon.jpg — original 256×256
SHA-256: 858ebcb92444859e77ba2485a9532bedda25254ddc00d3fca72517eb5ff38cca
```

O NRO novo voltou ao mesmo hash local registrado na Missão 1, como esperado para fontes/ícone inalterados no mesmo ambiente. Isso é resultado da compilação atual, não reaproveitamento do binário antigo como prova. Builds GitHub em outro diretório podem ter hashes próprios por dados de debug; seus sidecars são a referência para aquele download.

O pacote de SD e os checksums foram recriados e validados. O teste adicional abriu o NRO, localizou seu ASET e comparou exatamente o asset JPEG com `assets/icon.jpg`. A suíte não depende dos pacotes opcionais de renderização ou lint.

## Revisão visual da documentação

A página foi efetivamente aberta em Chromium, com inspeção visual das capturas do cabeçalho, catálogo, instalação e controles. Não se tratou apenas de compilar Markdown.

| Tamanho da janela | Largura da página | Logotipo |
| --- | --- | --- |
| Desktop 1440×1000 | 1440, sem overflow horizontal | 192×192, original carregado 256×256. |
| Tablet 768×1024 | 768, sem overflow horizontal | Mesmas proporções. |
| Smartphone 390×844 | 390, sem overflow horizontal | Mesmas proporções. |
| Smartphone estreito 320×740 | 320, sem overflow horizontal | Mesmas proporções; título quebra naturalmente em duas linhas. |

Evidências visuais guardadas no repositório:

- [Prévia desktop do README](../validacao/missao2/readme-desktop.png).
- [Prévia de smartphone do README](../validacao/missao2/readme-mobile.png).

**Essas imagens são prévias da documentação, não capturas do aplicativo no Switch.** Foi usado um renderizador local compatível e CSS de Markdown do GitHub; não se declara aqui uma captura do HTML produzido pelo servidor do GitHub. O cabeçalho usa apenas elementos compatíveis (`p`, `img`, `h1`, `strong`, `br`, `a`), sem estilos remotos obrigatórios ou imagem fictícia do aplicativo.

## Estado do GitHub e limitação administrativa

O trabalho continua no [PR #1](https://github.com/TemudjinBR/switch-file-manager/pull/1), na branch `codex/reorganizacao-estabilizacao`. A execução de Actions da revisão final identifica o SHA validado e fornece o artefato `SwitchFileManager-validado`, com NRO, pacote e checksums. O envio é feito sem merge, edição direta da main, force-push ou publicação de Release.

Descrição e tópicos do campo **Sobre não foram alterados**. O conector não expõe essa edição e a CLI recebeu `Forbidden` ao acessar a API; a política do executor é restrita. A proposta e o estado realmente observado estão em [MISSAO_2.md](../MISSAO_2.md). É uma limitação externa declarada, não uma alteração supostamente concluída por editar o README.

## O que não foi aprovado no console

A especificação da Missão 2 trouxe um relato físico do proprietário, no Switch Lite, de **“Falha ao salvar login no cartão SD.”** na v0.10.8. A causa exata da persistência não foi confirmada. O erro permanece documentado e **não houve correção OAuth nem novo teste físico nesta execução**.

Continuam pendentes a matriz de interface/controles/toque, SD FAT32/exFAT, persistência real, SSL/OAuth Google, FTP prolongado, arquivos grandes/ZIP64, fila, abertura de NROs e suspensão/retorno. Ver [TESTES.md](../TESTES.md) e [PROBLEMAS_CONHECIDOS.md](../PROBLEMAS_CONHECIDOS.md). O sucesso de testes no host não resolve nem invalida o relato do console.
