# Validação histórica — Missão 3.2, candidata 0.10.12

## Estado inicial e escopo

Em 06/11/2026, branch limpa `codex/reorganizacao-estabilizacao`, HEAD **c78ce357891a50d66b70ad90c224c49d2e2bd0aa**, VERSION 0.10.11, PR #1 aberto/sem merge, Actions [42164373035](https://github.com/TemudjinBR/switch-file-manager/actions/runs/42164373035) verde. A base pedida d66994c/0.10.10 era ancestral: as correções posteriores foram preservadas. Main era `db81e91ff73852a933f8c93a5d4177c3a98de2f0`. A suíte inicial com o NRO anterior passou **138/138**, em 69,828 s.

O anexo atualizado aprova FTP isolado/antes/depois do Drive e acesso/persistência/reconexão Drive; reprova simultaneidade, retomada e versão exibida. Isso retifica o relato inicialmente usado na [primeira rodada 3.2](../historico/VALIDACAO_MISSAO_3_2_INICIAL.md). Não reconstruímos o FTP. Históricos das Missões 1/2/3/3.1 continuam preservados; nenhuma nova aprovação física foi fabricada.

## Causas comprovadas e hipóteses

- **SDK/código:** libnx 4.12.0 configura três sessões BSD. `_bsdDispatchImpl` mantém uma sessão durante `svcSendSyncRequest`; `sessionmgrAttachClient` espera sem timeout quando todas estão ocupadas. Vários recv/select FTP bloqueantes podem impedir outra chamada de obter sessão. É evidência do mecanismo de contenção, **não prova da causa física do Lite**. Fontes primárias: [socket.c](https://github.com/switchbrew/libnx/blob/7644c9b26099aa2d2145bc72a21ee24190e92085/nx/source/runtime/devices/socket.c), [bsd.c](https://github.com/switchbrew/libnx/blob/7644c9b26099aa2d2145bc72a21ee24190e92085/nx/source/services/bsd.c), [sessionmgr.c](https://github.com/switchbrew/libnx/blob/7644c9b26099aa2d2145bc72a21ee24190e92085/nx/source/sf/sessionmgr.c).
- **UI/transporte:** Drive executava operações síncronas sem consultar controles durante navegação/envio em espera; SSL bloqueante do SDK pode esperar minutos. O callback interativo cobre TCP/TLS e preserva validação de certificados. DNS/SD ainda dependem do sistema.
- **Recuperação:** 0.10.11 já implementava checkpoints/CRC/revisão/Range/backoff. A rodada acrescenta espaço livre, CRC mais rápido compatível e escolha de reinício. Os cenários POSIX reproduzem perda de corpo/rede e recuperação; a causa específica da queda física ainda não foi medida no console.
- **Versão:** NACP dos builds anteriores estava correto nos artefatos validados. Não há caminho/hash do aparelho para provar cópia antiga, launcher ou instalação. Mantemos build/caminho e busca somente leitura; candidata 0.10.12 diferencia a nova execução.

Memória/applet, limites de sessões/serviços no firmware, SSL nativo, cartão e latência/conectividade real permanecem hipóteses verificáveis pelo diagnóstico. Não se atribui falha física a uma delas sem registro do Lite.

## Correções e preservação

Runtime solicita oito sessões BSD, buffers padrão, fallback para três apenas em falta explícita de recursos e diagnóstico do orçamento efetivo. FTP usa sockets não bloqueantes/select instantâneo e espera fora do IPC. Falha de thread publica diagnóstico antes de devolver 421 ao cliente, eliminando a corrida observada na regressão. O worker fecha seu próprio socket; a UI não faz shutdown sobre descritor potencialmente reutilizado.

UI/Drive permanecem na thread principal, sem API Drive reentrante. Callback desenha/lê controles durante espera: Y inicia/para FTP com consentimento anônimo e B cancela Drive. Parada Drive pendente marca cancelamento e adia liberação de SSL/runtime até destruir a conexão; FTP mantém sua posse. Não há navegação livre de painéis durante transferência nem operações Drive paralelas.

Downloads consultam statvfs para bytes restantes mais margem; falha SD não é erro Google. CRC32 por tabela mantém SFMDL2 compatível. Mudança de revisão/Range recusado oferece Preservar e voltar / Reiniciar com seguranca; o parcial anterior fica preservado. Nenhum upload passa a ter retomada persistente.

OAuth/persistência e caminhos legados, dois painéis, logo oficial, porta 2121, ZIP e operações anteriores preservados. Os testes usam somente arquivos e contas fictícios. Não houve acesso/modificação ao SD pessoal ou ao projeto separado Switch-Manager.

## Testes e compilação

| Verificação final local | Resultado |
| --- | --- |
| Suíte antes do build | 151 casos: 149 aprovados, dois dependentes de NRO ignorados, zero falhas; 109,048 s |
| Suíte completa com NRO final | **151/151 aprovados**, zero falhas/ignorados; 96,354 s |
| Rede/autenticação | 51 casos, incluindo persistência/contas/renovação/401 e falhas SD |
| Simultaneidade | Nove casos com os serviços no mesmo processo, comparação de bytes e timeout de deadlock |
| ThreadSanitizer | Os mesmos nove cenários passaram sem relatório de corrida; 29,589 s, apenas C++ host |
| Gerenciador | 21 casos, incluindo ENOSPC, recusa de reinício e prefixo esparso grande |
| Ciclo de vida | Nove casos; regressão do diagnóstico 421 aprovada após correção |
| Build real | devkitA64 r29.2-1/libnx 4.12.0-1, via `/workspace/sfm-environment/build.sh`, cópia descartável |
| NRO/NACP | **911.064 bytes**, título Switch File Manager, versão **0.10.12**, segmentos/ASET/NACP aprovados |
| Identidade dos fontes | **v0.10.12-9cf3464174eb**, conferida no NRO e manifesto |
| Ícone oficial | Comparado byte a byte no NRO; SHA-256 preservado |
| Distribuição | ZIP determinístico/lista permitida, manifesto/caminho/Android e SHA-256 aprovados; sem credenciais reais |
| Documentação | Links/âncoras/imagens de 30 documentos válidos; Markdownlint em 18 alterados, zero erros |
| Workflows/Git | actionlint 1.7.7 e diff --check aprovados; build não reescreve fontes |

Uma rodada intermediária reprovou um caso: o cliente recebia 421 antes da publicação do evento de erro de thread. A ordem foi corrigida no servidor, sem introduzir espera artificial no teste. A tabela usa a suíte completa posterior à correção; não reaproveita a aprovação do NRO anterior. Avisos restantes da compilação são funções estáticas não usadas do stb_truetype e limite de arquivos grandes do miniz, sem erro de build.

SHA-256 **local** do NRO final:

```text
a43d0559d4f7fbad0a640e38bf9ed270b19be9dd4ed1e4ee0a473693001f76f8
```

SHA-256 **local** de SwitchFileManager-v0.10.12.zip:

```text
8dfbc5932270e78af49b266ca2a2c41118d7eac2adc8ede88f939a29da5a5e2c
```

Imagem fixada: `devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282`. Builds remotos podem diferir por caminhos de debug: usar o sidecar do artefato baixado; a identidade dos fontes deve continuar igual.

Testes novos usam **os dois serviços no mesmo processo**, com barreiras HTTP: FTP deve completar e ter seus bytes comparados antes de liberar a resposta Drive. Incluem navegação/download/upload em múltiplos PUTs, quatro clientes e transferências paralelas, parada/reinício independente, cancelamento e timeout observável. Isso não simula IPC/SSL/applet.

O cenário grande cria prefixo esparso de 1,2 GiB, lê/confere seu CRC real e observa Range para remoto fictício de 2 GiB. Não baixa 2 GiB nem mede SD físico. ENOSPC, falha TLS/timeout/certificado, Range 200/206/416, mudança remota, 401, pausa/cancelamento/reinício e preservação de destinos são cobertos pela suíte, em fronteiras explicitadas em [TESTES](../TESTES.md).

## Arquivos desta rodada

- Rede: `src/network/socket_runtime.cpp`, `wifi_transfer.cpp` e stub/harness de ciclo de vida.
- Drive/UI: `src/cloud/gdrive.cpp`, `include/gdrive.h`, `src/app/main.cpp`.
- Downloads: `src/cloud/download_manager.cpp`, `include/cloud/download_manager.h`, harness e testes Python.
- Concorrência: `tests/host/concurrent.cpp`, `tests/test_concurrent_network.py`.
- Versão/pacote: `VERSION`, `scripts/package.py`, notas v0.10.12; Makefile/prepare/NACP/inspeção estabilizados preservados.
- Documentos: README, CHANGELOG, índice/roadmap, arquitetura, referência, FTP, Drive, downloads, controles, funcionalidades, testes, problemas, instalação Android, histórico, relatório e checklist.

## GitHub e validação física

O [PR #1](https://github.com/TemudjinBR/switch-file-manager/pull/1) permanece aberto. A execução específica e o artefato final serão ligados aqui/na descrição do PR após CI. Nenhum merge, alteração direta na main ou Release está autorizado nesta missão.

O [checklist físico com Funcionou / Não funcionou](../VALIDACAO_SWITCH_LITE_3_2.md) é obrigatório para avaliar a candidata. FAT32/exFAT, SSL Google real, applet/memória completa, UI/toque, rendimento/estabilidade, suspensão e queda elétrica não são aprovados por compilação ou testes de host. Não executar testes destrutivos no cartão pessoal.
