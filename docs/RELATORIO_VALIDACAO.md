# Validação — Missão 3 (05/11/2026)

## Escopo e estado inicial

Revisão experimental **0.10.9** na branch `codex/reorganizacao-estabilizacao`, PR #1 preservado. Base limpa inicial: `97382fc9d2cbee8f9dddabafccf3933da52a486a`; CI anterior 42113511214 aprovado. Main permanece em `db81e91ff73852a933f8c93a5d4177c3a98de2f0`. Relatórios das [Missões 1](historico/VALIDACAO_MISSAO_1.md) e [2](historico/VALIDACAO_MISSAO_2.md) preservados.

O escopo é persistência/autenticação Drive e seus testes/documentos. Não foram alterados logotipo, organização estabilizada, operações pessoais, FTP ou ZIP. Sem merge e sem Release.

## Causa comprovada e hipótese física

Um executável da base anterior com rename recusando destino existente reproduziu “Falha ao salvar login no cartão SD.” ao receber HTTP 200 e substituir token fictício existente. O arquivo anterior permaneceu intacto. A leitura da fonte oficial do libnx 4.12.0 confirmou a diferença de rename, fsync via fsFileFlush e lstat via stat. SDK efetivo: libnx 4.12.0-1, devkitA64 r29.2-1.

Isso comprova uma incompatibilidade condicional. Não havia errno do relato do Lite, portanto não prova qual operação falhou no cartão daquele console. Diretórios, espaço, permissões, escrita, flush, fechamento e releitura continuam hipóteses físicas. Detalhes: [contrato de persistência](GOOGLE_DRIVE_PERSISTENCIA.md).

## Resultados desta revisão

**72/72 testes passaram com o NRO novo, sem casos ignorados**. A suíte anterior à compilação reconheceu 72 casos: 70 passaram e dois dependentes de binário foram explicitamente ignorados. A revisão documental encontrou inicialmente uma frase histórica removida do roadmap; ela foi restaurada e a execução completa foi repetida com sucesso.

| Verificação | Resultado desta execução |
| --- | --- |
| `bash scripts/build.sh` | Suíte no host, build real, validação NRO, suíte com NRO e pacote concluídos |
| Suíte anterior ao build | 72 casos, 70 aprovados/2 ignorados; 27,888 s |
| Suíte com NRO | 72 aprovados, zero ignorados; 28,875 s |
| Armazenamento | 14 métodos com subcasos de falha; rename sem sobrescrita, rollback/recuperação |
| Rede/Drive/FTP | 25 testes; primeiro login/renovação, retry, erro SD após HTTP 200 e preservação de dados fictícios |
| NRO | 800.472 bytes; NRO0, segmentos/ASET, JPEG/NACP, título e versão 0.10.9 aprovados |
| Distribuição | ZIP/lista fechada, checksums e repetição determinística do pacote aprovados pela suíte e conferência dos sidecars |
| Documentação | 24 arquivos ativos, links locais/âncoras/imagens válidos; Markdownlint em 23 arquivos, zero erros |
| Workflows | actionlint 1.7.7 aprovado; CI existente mantido, inclui novos testes automaticamente e publica `dist/*` |
| `git diff --check` | Aprovado |
| Ícone | Original preservado e comparado byte a byte no NRO |

Compilação pelo `/workspace/sfm-environment/build.sh` em cópia descartável, com imagem:

```text
devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282
```

Hashes desta compilação local:

```text
SwitchFileManager.nro (800472 bytes)
726f9e8e4fb7e6f147f42fb6f31800a0b771d0f374d931b2f3a654a8ed840338

assets/icon.jpg
858ebcb92444859e77ba2485a9532bedda25254ddc00d3fca72517eb5ff38cca
```

Avisos restantes são de funções estáticas não usadas em stb_truetype e suporte de grandes arquivos em miniz; nenhum erro de compilação. Nenhum fonte precisou ser reescrito pelo build. Builds em diretórios distintos podem ter hash próprio por informações de debug; conferir o checksum que acompanha cada artefato.

## GitHub e executável experimental

O [PR #1](https://github.com/TemudjinBR/switch-file-manager/pull/1) recebe os commits desta missão sem substituir o histórico. O [GitHub Actions](https://github.com/TemudjinBR/switch-file-manager/actions/workflows/build.yml) associa resultado, SHA validado e download `SwitchFileManager-validado`. O artefato contém NRO, ZIP para SD e checksums, sem configurações/credenciais. O resumo do PR identifica a execução remota final; um artefato de Actions não é uma Release.

Não houve alteração de main, merge, force-push ou publicação de Release.

## Limites físicos

Não houve teste em Nintendo Switch, Google real, SD do proprietário, FAT32/exFAT real ou perda elétrica. O sucesso no host e a compilação não aprovam o login no hardware. A matriz física inclui primeira autorização, token existente/rotacionado, encerramento/reabertura, renovação, desconexão, nova tentativa e diagnóstico real de etapa/errno, usando dados de teste sem operações destrutivas.
