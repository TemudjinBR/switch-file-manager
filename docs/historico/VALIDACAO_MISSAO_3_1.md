# Validação — Missão 3.1

## Estado inicial confirmado

Branch `codex/reorganizacao-estabilizacao` limpa em `b9f104a41009afc910907e87d50e5d35f102e64a`, PR #1 aberto/sem merge; Actions 42122757086 aprovado. Main em `db81e91ff73852a933f8c93a5d4177c3a98de2f0`. Suíte inicial reexecutada com NRO 0.10.9: 72/72 aprovados. Missões [1](VALIDACAO_MISSAO_1.md), [2](VALIDACAO_MISSAO_2.md) e [3](VALIDACAO_MISSAO_3.md) preservadas.

## Causas e implementação

FTP: duas inicializações independentes de socketInitializeDefault e encerramentos independentes entre FTP/Drive. Libnx recusa a segunda inicialização de soc:; o conflito foi reproduzido com fronteira SDK simulada. O Result do relato físico não foi recebido, portanto outras falhas de memória/serviço/Wi-Fi continuam possíveis. O gerenciador compartilhado corrige a posse e registra Result real, módulo/descrição e contexto de applet/memória. [Detalhes](../FTP.md).

Versão: o NRO anterior contém NACP 0.10.9. A leitura 0.10.8 no console não foi reproduzida com aquele binário; caminho/cópia antiga permanecem hipóteses. Foram protegidos VERSION, geração NACP por conteúdo, identificação em execução e pacote; acrescentada inspeção somente leitura de cópias. [Detalhes](../VERSAO_E_INSTALACAO.md).

Drive: menu completo, preferência, restauração, desconexão, reconexão, troca com commit verificado/cancelamento e esquecimento seletivo. PKCE S256/state, estados distintos e identidade opcional. Arquivo único de conta tem precedência sobre legados preservados; sem exclusão automática por falha OAuth. Slots continuam após 127; cópias não são expurgadas sem ação explícita. [Contrato](../GOOGLE_DRIVE_PERSISTENCIA.md).

## Validações por etapa

- Base: 72/72 testes com NRO anterior, antes das alterações.
- Rede: cinco testes novos de ciclo de vida e compilação nativa; 77/77 casos com o NRO da etapa.
- Contas: 41 testes de rede/Drive; compilação nativa intermediária de 833.240 bytes e 97/97 casos com NRO.
- Final 0.10.10: **106/106 testes aprovados com NRO novo, zero reprovados/ignorados**; execução de 53,406 s. Antes do build: 106 reconhecidos, 104 aprovados e dois dependentes de NRO explicitamente ignorados, 52,851 s.

## Compilação final e artefatos

`bash scripts/build.sh` executou suíte no host, `/workspace/sfm-environment/build.sh`, validação NRO, suíte com binário e pacote. Build nativo libnx 4.12.0-1/devkitA64 r29.2-1, imagem fixada:

```text
devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282
```

| Verificação | Resultado observado |
| --- | --- |
| NRO/NACP final | 833.240 bytes, DisplayVersion 0.10.10, título Switch File Manager, NRO0/segmentos/ASET aprovados |
| Testes com binário | 106/106 aprovados; regressões de rede, protocolo, contas, arquivos, ZIP, logs, versão e distribuição |
| Ícone | Hash oficial preservado e bytes comparados dentro do NRO |
| Pacote | Lista fechada, ZIP íntegro, manifesto com caminho, ausência de segredos fictícios e checksums aprovados |
| Documentação | 26 arquivos ativos com links/âncoras/imagens válidos; Markdownlint em 25 arquivos, zero erros |
| Workflows | actionlint 1.7.7 aprovado; workflows existentes incluem novos testes automaticamente |
| Inspeção instalada | NRO local identificado como 0.10.10; duplicados antigos detectados em fixture somente leitura |
| `git diff --check` | Aprovado |

```text
SwitchFileManager.nro — 833240 bytes
SHA-256: 6c2c1f58233b588cef3435ca58cb66ff942a973e04256006e6ee1ed36969ad61

assets/icon.jpg
SHA-256: 858ebcb92444859e77ba2485a9532bedda25254ddc00d3fca72517eb5ff38cca
```

Avisos normais do build vêm de funções estáticas não usadas em stb_truetype e suporte a grandes arquivos em miniz. Nenhum erro de compilação. Builds remotos podem ter hash diferente por caminhos de debug; conferir o sidecar do download específico.

### Conferência nativa adicional do NACP

Na cópia descartável já compilada, nacptool real gerou um NACP 0.10.8 e recebeu timestamp futuro intencional. Em seguida, make foi executado com APP_VERSION=0.10.8. O NRO resultante foi extraído/validado como **0.10.10**, com o mesmo SHA-256 final. Fontes, headers, Makefile e VERSION permaneceram idênticos. O aviso de clock skew nessa reprodução é esperado pelo timestamp futuro deliberado; a conferência posterior do conteúdo NACP/NRO passou.

### Reprodução do FTP anterior

Foi compilado wifi_transfer.cpp da base b9f104a com a fronteira SDK simulada. Após iniciar sockets como um consumidor anterior, o FTP antigo recusou inicializar e retornou exatamente “Falha ao inicializar a rede”. A implementação nova passou pelas transferências/ciclos compartilhados. Esse experimento não produz o Result real do Switch nem transforma a hipótese física em causa exclusiva.

## Arquivos e responsabilidades alterados

| Local | Alteração |
| --- | --- |
| src/network/socket_runtime.cpp, include/network/socket_runtime.h | Posse compartilhada e diagnóstico SDK |
| src/network/wifi_transfer.cpp | Integração do FTP com o ciclo comum |
| src/cloud/gdrive.cpp, include/gdrive.h | Estado/transação de conta, preferência, OAuth e erros |
| src/cloud/credential_store.cpp, include/cloud/credential_store.h | Slots descobertos, recuperação e exclusão seletiva explícita |
| src/security/oauth.cpp, include/security/oauth.h | Nonces e PKCE S256 |
| src/app/main.cpp | Menu, confirmações, painéis, restauração e identificação |
| VERSION, Makefile | Candidata 0.10.10 e NACP autoritativo |
| scripts/package.py, scripts/inspect_installation.py | Instalação identificável e inspeção somente leitura |
| tests/host, tests/stubs, testes Python novos/ampliados | Fronteiras fictícias e regressão real dos módulos |
| .gitignore | Estado de conta/cópias fora do Git |
| README, CHANGELOG, docs, release-notes/v0.10.10.md | Guias, evidências, limites e continuidade |

Os arquivos de FTP de protocolo, ZIP, logo oficial e terceiros não foram reescritos. A estrutura modular permanece; não houve acesso ao projeto separado Switch-Manager, dados pessoais ou credenciais reais.

## GitHub e limitações

O [PR #1](https://github.com/TemudjinBR/switch-file-manager/pull/1) recebe os commits sem force-push. O resultado remoto/artefato da revisão exata fica no PR e no [Actions](https://github.com/TemudjinBR/switch-file-manager/actions/workflows/build.yml), com NRO experimental, ZIP e checksums. Sem merge, modificação direta de main ou Release.

Não houve teste físico no Lite, cartão pessoal, TLS Google real ou energia do SD. A matriz em [TESTES.md](../TESTES.md) exige modo applet/memória completa, versão/caminho, FTP antes/depois de Drive, preferência e reinício, troca/cancelamento e esquecimento com conta/dados de teste. Nenhum resultado no host comprova esses cenários no hardware.
