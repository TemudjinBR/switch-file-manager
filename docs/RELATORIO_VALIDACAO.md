# Validação — Missão 3.3, atualizações e distribuição

## Base, estado remoto e preservação

Base limpa **7116e3415e808dc8a21f0ce2318a264c1960b3cd**, candidata 0.10.12, branch codex/reorganizacao-estabilizacao. PR #1 aberto/sem merge; [Actions 42241739165](https://github.com/TemudjinBR/switch-file-manager/actions/runs/42241739165) aprovado. Main em db81e91ff73852a933f8c93a5d4177c3a98de2f0. A suíte inicial com NRO anterior passou **151/151** em 95,858 s. [Relatório 3.2 preservado](historico/VALIDACAO_MISSAO_3_2_SIMULTANEIDADE.md).

O anexo 3.3 foi tratado como a missão solicitada, preservando os resultados físicos anteriores: FTP isolado/sequencial e acesso/persistência/reconexão Drive aprovados pelo proprietário; simultaneidade/retomada/versão e a nova atualização ainda dependem de hardware. Nenhuma nova homologação foi presumida.

## Implementação

- Canal estável padrão, experimental explícito e preferências persistidas, checagem ao abrir opcional com intervalo, consulta manual, notas, versão/tamanho/estado/bytes e cancelamento de tarefa em worker independente.
- Transporte HTTPS próprio, contexto SSL separado e terceira posse do runtime, sem reentrar em OAuth/Drive. Certificados/nome/data verificados; timeouts e respostas parciais/limites tratados. Consulta anônima, sem token pessoal no NRO.
- Manifesto oficial e SHA-256 incremental de principal/auxiliar, comparação de versão, metadados NRO/NACP/build e caminho oficial conferidos. Sem assinatura independente: confiança HTTPS/GitHub explicitada em [ATUALIZACOES](ATUALIZACOES.md).
- Áreas exclusivas, journal, hash anterior, backup e instalação somente em NRO auxiliar após encerrar o principal; idempotência, recuperação e rollback explícitos. Nenhuma cópia duplicada é removida. SD real/corte elétrico não são simulados por fsync/rename no host.
- Makefile compila os dois NROs; pacote contém ambos, manifestos, instruções Android e checksums. Fontes do auxiliar entram na identidade do build.
- Workflows manuais de experimental e promoção estável; gate de proteção, revisão, último CI exato, relatório físico e aprovação do proprietário vinculada a SHA/versão/hashes. Não há promoção por CI verde, force-push, retag ou sobrescrita de Release.

## Evidências e restrições do ambiente

Pesquisa do [nx-hbloader](https://github.com/switchbrew/nx-hbloader/blob/82b95122c5ae8dc059bf23893ba7623c72c86773/source/main.c) confirma leitura/fechamento do NRO antes de mapear/saltar. libnx expõe envHasNextLoad/envSetNextLoad. O projeto usa auxiliar separado para não trocar seu próprio executável; isso não dispensa teste da passagem/retorno no Lite/Atmosphère real.

A API de branch retornou main **protected: false**, enforcement off e contexts vazios em 06/11/2026. O conector fornece essa leitura, mas nenhuma ferramenta administrativa de proteção de branch foi disponibilizada. Não houve PUT de proteção nem rejeição 403 correspondente; não atribuir a ausência a credenciais sem essa evidência. O host api.github.com não está autorizado pela política do CLI do ambiente; não se tentou contorná-la. Não foram solicitados tokens pessoais.

A proteção **não foi configurada**: o workflow recusa promover nessa condição. Também faltam homologação física, comentário de aprovação e relatório aprovado para a candidata. Esses gates não impedem CI/artefato experimental, mas impedem primeira promoção. [Processo e condições](PROMOCAO_E_RELEASES.md).

## Testes e build

| Verificação final local | Resultado |
| --- | --- |
| Suíte sem NRO | 192 casos: 189 aprovados, três dependentes de binário ignorados; zero falhas; 112,150 s |
| Suíte com NRO/auxiliar reais | **193/193 aprovados**, zero falhas/ignorados; 112,141 s |
| Atualizador | 25 casos: canais/versões/origem, API/rate limit, chunked/parcial/checksum, corrupção, caminhos, transação/rollback/crashes e FTP real com download pendente |
| ThreadSanitizer | **25 cenários aprovados**, sem relatório de corrida no C++ host; 30,690 s |
| Política/controlador | Oito casos de gates e seis de controlador; API/processos fictícios, sem publicação ou merge reais |
| Rede/autenticação anterior | Regressões mantidas, incluindo 51 cenários de rede/contas, nove de simultaneidade FTP/Drive e dez de ciclo de vida com terceiro dono Updates |
| Compilação real | devkitA64 r29.2-1/libnx 4.12.0-1, wrapper configurado, mesma imagem fixada por digest da 3.2 |
| Principal | **1.033.944 bytes**, NACP 0.10.13, título Switch File Manager |
| Auxiliar | **386.776 bytes**, NACP 0.10.13, título Atualizador do Switch File Manager |
| Build dos dois NROs | **v0.10.13-cc76cad73843**, correspondente aos fontes |
| Distribuição | NROs, ZIP para SD, manifestos e sidecars verificados; ícone oficial comparado byte a byte nos dois NROs |
| Documentação/workflows | 34 documentos com links/âncoras/imagens válidos; Markdownlint em 17 alterados e actionlint aprovados |
| Git | diff --check aprovado; build não reescreve fontes; artefatos ignorados pelo versionamento |

A execução final foi `bash scripts/build.sh`: suíte host, compilação real dos dois NROs, validadores NRO/NACP/build, suíte com binários e empacotamento. Após o ajuste da entrada manual registrada e seu novo teste de controlador, os dois NROs foram recompilados e a suíte completa repetida: 193/193 aprovados. O SHA do ícone oficial continua 858ebcb92444859e77ba2485a9532bedda25254ddc00d3fca72517eb5ff38cca. Warnings restantes são funções estáticas não utilizadas do stb_truetype e a nota de arquivos grandes do miniz; o aviso do compilador sobre std::sort em array pequeno foi eliminado por verificação direta dos três pares de assets.

Hashes **locais** (usar os sidecars do artefato remoto específico ao baixar):

```text
SwitchFileManager.nro
fc00935b8171d306d4cd07af947e0addbe95d63abfcdfdf8861b54693fdbdd85

SfmUpdateHelper.nro
0149f99bb56c7e34a002271d85c4353902afb5c205f6492b788972cba9654284

dist/SwitchFileManager-v0.10.13.zip
874d4cc7c05b8fc279b48d878f4b0ee16c2e55d709acf63ff30491af56a857d0

```

Caminhos de debug podem mudar hashes entre build local e remoto; build/versão devem coincidir. O manifesto local de desenvolvimento pode ter commit zero, que não é elegível para publicação; no Actions, SFM_SOURCE_COMMIT recebe o SHA do checkout. A mensagem “Release idêntica já publicada” nos testes vem da API fictícia de idempotência, não de uma Release real.

Os novos testes usam SHA comparado com hashlib, parser/manifestos e E/S reais temporários, sem dados pessoais; HTTP loopback testa seleção de canal, resposta parcial, chunked, corrupção, cancelamento, erro de rede/limite e responsividade do worker. Falhas/crashes entre renomeações exercitam recuperação/rollback com semântica fsdev sem sobrescrita. API/subprocessos fictícios testam publicação idêntica como no-op e ausência de merge/tag/Release quando gates falham. A suíte anterior de FTP, Drive/OAuth, ZIP, logs, identidade e distribuição permanece obrigatória.

## Arquivos principais

| Área | Arquivos |
| --- | --- |
| Núcleo/worker | include/updates/*, src/updates/update_core.cpp, update_service.cpp |
| HTTPS/runtime | include/network/socket_runtime.h, src/network/socket_runtime.cpp, src/network/update_http.cpp |
| Auxiliar/UI | tools/updater/updater_main.cpp, src/app/main.cpp |
| Persistência | include/cloud/credential_store.h, src/cloud/credential_store.cpp; novo arquivo update-settings.json em pasta separada |
| Build/pacote | VERSION, Makefile, .gitignore, scripts/prepare.py, validate_nro.py, package.py, build.sh |
| Publicação | scripts/release_policy.py, github_distribution.py, workflows build.yml/release.yml/experimental.yml/validate.yml e scripts/ci_build.sh |
| Testes | harness updater, test_updater, test_release_policy, test_distribution_controller, distribuição e ciclo de vida |
| Documentação | README, CHANGELOG, guias técnicos, atualização/promoção, checklist 3.3, histórico e notas v0.10.13 |

Logo, terceiros, Commander, FTP, Drive e dados legados preservados. O projeto separado Switch-Manager e o SD pessoal não foram alterados. O wrapper devkitPro externo ao repositório foi ajustado somente para copiar também o auxiliar gerado, sem mudar imagem ou SDK.

## GitHub e pendências físicas

O [PR #1](https://github.com/TemudjinBR/switch-file-manager/pull/1) registra commits, execução Actions e artefato experimental exatos ao concluir a validação. Não foi acionado workflow de publicação, merge, tag ou Release nesta missão.

O [checklist 3.3](VALIDACAO_SWITCH_LITE_3_3.md) exige TLS GitHub real, worker/controles/toque, instalação no alvo correto, passagem para auxiliar, retorno, backup/rollback, applet/memória completa, FAT32/exFAT e regressões das funções aprovadas. Assinatura independente, retomada do download do atualizador, limpeza automática de áreas antigas e retomada de upload Drive não foram implementadas.

## Entrada manual antes do primeiro merge

A API confirmou que somente build.yml existe na main original. Segundo a regra do GitHub, workflow_dispatch exige arquivo registrado na branch padrão. Por isso o workflow **Compilar e testar Switch File Manager** também expõe entradas manuais na branch do PR: publish_experimental (padrão falso) e promote_stable (padrão falso, chama o controlador reutilizável com os mesmos gates). Selecionar a branch codex/reorganizacao-estabilizacao e o SHA correspondente; informar PR/comentário para promoção. A opção estável tem prioridade se ambas forem marcadas. Consultas normais do PR continuam somente testando/compilando, sem publicar.

Isso prepara a primeira entrada sem adicionar arquivos à main antecipadamente. release.yml usa validate.yml para a compilação interna; não chama build.yml de volta, evitando um ciclo de workflows. Ambos os caminhos de compilação executam scripts/ci_build.sh. Publicações usam a mesma exclusão mútua sfm-distribuicao. Os disparos reais de publicação não foram executados: proteção/homologação continuam ausentes e bloqueiam estável. [Regra workflow_dispatch](https://docs.github.com/actions/using-workflows/events-that-trigger-workflows#workflow_dispatch).
