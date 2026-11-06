# Canais, homologação e promoção

## Estado desta missão

A candidata 0.10.13 é experimental no PR #1. A 0.10.12 não foi homologada como estável; não houve merge, tag, pré-Release ou Release nesta missão. O CI produz artefato para instalação inicial Android. Os workflows de publicação foram preparados para execução futura pelo proprietário.

A API consultada em 06/11/2026 retornou `protected: false` para main, com enforcement `off` e nenhum check obrigatório. Isso é evidência de configuração, não erro de autenticação. Os conectores disponíveis permitem leitura/edição de PR, mas não disponibilizam edição de proteção de branch. A política do ambiente não autoriza o host api.github.com para o CLI; não houve PUT administrativo nem resposta 403 a essa operação para atribuir à autenticação. Não se pediram tokens pessoais e não se contornou a política. **Proteção não foi aplicada; a promoção permanece bloqueada por esse gate real.**

## Política de versão e canais

VERSION X.Y.Z (até 15 caracteres) alimenta executável, NACP, build, manifesto e pacote. Tag/Release usam vX.Y.Z. O canal é metadado de distribuição, não outro executável com versão falsificada.

- Artefato do CI: sempre experimental por padrão, sem publicação/merge automáticos.
- Pré-Release experimental pública: workflow **Disponibilizar pré-Release experimental**, disparo manual do proprietário com SHA completo do head da branch de trabalho; recompila/testa e publica somente com `--prerelease`.
- Estável: workflow **Promover versão homologada**, com aprovação física estruturada do proprietário para commit/binários exatos. CI verde sozinho não basta.
- Não reutilizar número para conteúdo ou canal diferente. Se vX.Y.Z já foi publicado experimentalmente, ele não é convertido em estável; a próxima estável exige outra versão e homologação. Isso evita substituição de manifestos/assets sob uma mesma tag.
- Reexecução de publicação com tag/commit/canal/assets idênticos é no-op, comparando os digests SHA-256 fornecidos pela API GitHub. Colisão, digest ausente, Release incompleta ou tag divergente bloqueiam; nunca usar force-push, mover tag ou `--clobber`.

## Gates estáveis

Antes de publicar, o controlador exige:

1. Proprietário acionando workflow no repositório oficial; PR do próprio repositório, não draft, base main e head exato informado. PR já promovido só permite retomada/idempotência com a mesma identidade e ancestralidade na main.
2. main protegida, com checks **tests** e **build** obrigatórios para todos, inclusive administradores (`enforcement_level: everyone`). Proteções reais do GitHub continuam valendo no endpoint de merge; o workflow não usa bypass.
3. Último CI **Compilar e testar Switch File Manager** do SHA exato aprovado; nova compilação/testes completos da candidata imutável.
4. Relatório `docs/validacao-console/vX.Y.Z.md`, com dispositivo Lite, Atmosphère, formato SD, resultado APROVADO, hashes reais dos dois NROs e nenhum caso PENDENTE/NAO EXECUTADO. Não existe relatório fictício aprovado para esta candidata.
5. Comentário do proprietário no mesmo PR com aprovação expressa de revisão, teste físico e merge/Release, vinculado ao SHA/versão/hashes. Autorização geral da missão não substitui esse registro.
6. README/CHANGELOG/notas correspondentes à versão. O README precisa conter a declaração `Canal estável: vX.Y.Z` da promoção; ajustar a vitrine/classificações antes de aprovar o commit final.
7. Artefato com lista fechada, manifestos coerentes, hashes dos NROs iguais aos homologados e número/tag inéditos (ou a mesma tag de uma tentativa interrompida, sem mover referência).

A política exigida de branch deve ser aplicada por uma capacidade administrativa autorizada do GitHub. Como essa capacidade não está disponível nesta sessão, não foi simulada nem substituída por uma flag local.

## Registro da homologação

Somente depois do checklist físico e revisão, o proprietário pode registrar no PR:

```text
HOMOLOGACAO_SFM_1
{"commit":"SHA_COMPLETO_TESTADO","version":"X.Y.Z","nro_sha256":"HASH_REAL_64_HEX","helper_sha256":"HASH_REAL_64_HEX","reviewed":true,"physical":true,"authorize_merge_release":true}
```

Esse bloco é **um modelo inválido para execução**, não uma aprovação. O comentário é lido pela API, e sua autoria/PR são conferidos. O relatório físico deve registrar também build, caminho instalado, observações e resultados das funcionalidades aplicáveis. Atualizar somente documentação não muda o build incorporado, mas muda o commit: a autorização precisa vincular o head final e os hashes reais correspondentes.

## Merge e publicação futuros

`release.yml` valida gates com token de leitura, chama `build.yml` para o SHA imutável/canal stable e revalida antes de qualquer escrita. No job de publicação, GITHUB_TOKEN recebe apenas contents/write, pull-requests/write e leituras necessárias. O token não é embutido nos NROs nem persistido pelo checkout.

O endpoint de merge recebe o head esperado. Após merge, confirma ancestralidade na main; a tag aponta ao commit homologado, agora alcançável na main, sem retag ou novo binário implícito. A Release recebe NRO principal, auxiliar, ZIP para SD, checksums, update.json e notas em PT-BR. Falha após merge/tag permite repetição do mesmo estado; Release parcial exige revisão em vez de sobrescrita silenciosa.

Não se executou merge/publicação para testar o controlador. Os testes usam API/subprocessos fictícios e verificam ausência de mutações em falha, idempotência e bloqueios. Os passos reais de merge, uploads de Release e proteção administrativa só poderão ser validados quando os gates forem satisfeitos. Ver [relatório](RELATORIO_VALIDACAO.md).

## Entrada manual antes do primeiro merge

A API confirmou que somente build.yml existe na main original. Segundo a regra do GitHub, workflow_dispatch exige arquivo registrado na branch padrão. Por isso o workflow **Compilar e testar Switch File Manager** também expõe entradas manuais na branch do PR: publish_experimental (padrão falso) e promote_stable (padrão falso, chama o controlador reutilizável com os mesmos gates). Selecionar a branch codex/reorganizacao-estabilizacao e o SHA correspondente; informar PR/comentário para promoção. A opção estável tem prioridade se ambas forem marcadas. Consultas normais do PR continuam somente testando/compilando, sem publicar.

Isso prepara a primeira entrada sem adicionar arquivos à main antecipadamente. release.yml usa validate.yml para a compilação interna; não chama build.yml de volta, evitando um ciclo de workflows. Ambos os caminhos de compilação executam scripts/ci_build.sh. Publicações usam a mesma exclusão mútua sfm-distribuicao. Os disparos reais de publicação não foram executados: proteção/homologação continuam ausentes e bloqueiam estável. [Regra workflow_dispatch](https://docs.github.com/actions/using-workflows/events-that-trigger-workflows#workflow_dispatch).
