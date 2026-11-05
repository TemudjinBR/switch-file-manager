# Relatório de validação — 05/11/2026

## Resultado desta execução

**37 testes aprovados, sem testes ignorados na execução com o NRO compilado.** O build devkitPro produziu `SwitchFileManager.nro` de **779.992 bytes**, com título `Switch File Manager` e versão NACP **0.10.8**. Não houve execução em Nintendo Switch físico e nenhuma Release foi publicada.

Os fontes de execução correspondem ao commit `b6a3a5b131bbff23f1a5fa15f36bd529a7836933`. Os commits seguintes consolidam documentação, distribuição e workflow; os checks do PR identificam a revisão final avaliada pelo GitHub.

| Verificação | Resultado |
| --- | --- |
| Checkout inicial, base db81e91 | Limpo; compilação original passou, NRO 775.896 bytes |
| Migrações incorporadas e preparação idempotente | Testes e build passaram |
| Migração das pastas e Makefile | Testes e build passaram |
| Extração de UI/caminhos/logger | Testes e build passaram |
| Extração das operações de armazenamento e correções | Testes e build passaram |
| Suíte final com artefato real | `python3 tests/run.py --nro SwitchFileManager.nro`: 37/37, 18,795 s nesta execução |
| NRO real | Segmentos, ASET, limites de assets, JPEG, NACP, título e versão aprovados |
| Distribuição | ZIP íntegro, conteúdo permitido, hashes e repetição determinística aprovados |
| `make` repetido na cópia já compilada | Nenhum objeto recompilado; SHA-256 do NRO inalterado |
| Mudança incremental de VERSION, somente em cópia descartável | 0.10.8 → 0.10.9 recompilou main/NACP e validou metadados 0.10.9 |
| Restauração de VERSION na mesma cópia | 0.10.9 → 0.10.8; NRO voltou ao mesmo SHA-256 original |
| `actionlint` 1.7.7 | Dois workflows passaram, sem diagnósticos após as correções |
| Sintaxe YAML e scripts | Aprovada; suíte principal não depende de PyYAML |
| `git diff --check` | Aprovado |
| Integridade de terceiros e ícone | Bytes idênticos à base auditada |
| Erro Git 128 | Reproduzido por diferença de propriedade; configuração exata de safe.directory passou |

## Artefatos locais verificados

```text
SwitchFileManager.nro — 779992 bytes
SHA-256: 9aa5dabdd87a426540b458fd78d36505ba9a51b9c2f3b3e18eb7d0ee012350cc

SwitchFileManager-v0.10.8.zip
SHA-256: 6f278fdd96ec28942f883694f2d1cf51107858964d9bc6fc5c4a66fb2396a4fe
```

O pacote contém `switch/Switch File Manager/SwitchFileManager.nro`, `LEIA-ME.txt`, `manifest.json` e `SHA256SUMS`. O diretório `dist/` também contém o NRO separado e sidecars SHA-256. Não contém configurações ou tokens. O manifesto declara validação física pendente.

A imagem usada foi:

```text
devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282
```

Os hashes acima são desta compilação local em nuvem. Um build do GitHub pode ter hash diferente por diretório/artefatos de debug; a execução remota gera seus próprios checksums. Não confundir determinismo do empacotamento com garantia de build binariamente reproduzível em qualquer ambiente.

## Falhas reproduzidas e corrigidas

- Drive: HTTP 206 com dados insuficientes era aceito e o destino era artificialmente ampliado. O teste falhou antes da correção e passou após contagem e validação do intervalo.
- Drive: HTTP 416 era tratado como download completo. Agora retorna falha e limpa somente a saída criada para esse download.
- Drive: HTTP 401 seguido de renovação misturava respostas JSON, podendo informar sucesso com pasta vazia. O teste confirmou o defeito; a nova resposta reinicia seu buffer.
- ZIP: escrita pequena em `/dev/full` só falhava no flush/close, que eram ignorados. O teste falhou antes e passou após a correção.
- Workflow: contexto Git sem confiança do diretório de outro UID retornava 128 em `git config`. A reprodução passou com confiança restrita ao workspace; também foi removida a necessidade de commit durante compilação.

A suíte adicional cobre preservação de destinos, cancelamento e credenciais fictícias, sem conta Google real. Resultados de protocolo são de POSIX/loopback, sem simulação de hardware ou SSL nativo.

## Como conferir no GitHub

O PR contém os commits e a documentação revisável. O workflow `Compilar e testar Switch File Manager` roda para o SHA do PR; seus jobs `tests` e `build` devem ficar verdes. O artefato **SwitchFileManager-validado** contém NRO, pacote e checksums. A presença de um artefato de CI não é uma aprovação para distribuição pública.

Os logs brutos do ambiente desta sessão foram mantidos em `/workspace/work/`: `baseline-build.log`, `infra-build.log`, `layout-build.log`, `modular-build.log`, `storage-build.log`, `safety-build.log`, `verified-build.log`, `final-tests.log`, `incremental-build.log`, `git-ownership-repro.log` e registros dos testes antes das correções. Esses caminhos são evidência da sessão, não dependências para continuar o projeto. O GitHub produz logs próprios reproduzíveis.

## Pendente no hardware

Interface, controles, toque, SD FAT32/exFAT, SSL e OAuth Google reais, desempenho/estabilidade FTP, arquivos grandes, fila, abertura de NROs e suspensão/retorno. A matriz completa e os critérios de Release estão em [TESTES.md](TESTES.md); limites técnicos em [PROBLEMAS_CONHECIDOS.md](PROBLEMAS_CONHECIDOS.md).
