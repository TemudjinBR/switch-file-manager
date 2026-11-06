# Validacao — Missao 3.2

## Base e resultados fisicos recebidos

Branch limpa codex/reorganizacao-estabilizacao em d66994c, versao 0.10.10; PR #1 aberto/sem merge, Actions 42135954850 aprovado. Main permaneceu db81e91ff73852a933f8c93a5d4177c3a98de2f0. A base foi reexecutada com o NRO anterior: **106/106 testes passaram**, sem ignorados. Historicos das [missoes 1](historico/VALIDACAO_MISSAO_1.md), [2](historico/VALIDACAO_MISSAO_2.md), [3](historico/VALIDACAO_MISSAO_3.md) e [3.1](historico/VALIDACAO_MISSAO_3_1.md) preservados.

O proprietario confirmou acesso Drive, Manter conectado e desconectar/reconectar. Reprovou FTP, FTP apos Drive, versao exibida e download apos perda de internet. Esses resultados sao relato fisico recebido; a equipe nao acessou o console/SD pessoal. **A 3.1 nao esta aprovada para producao.** [Matriz recebida e novo checklist](VALIDACAO_SWITCH_LITE_3_2.md).

## Causas comprovadas e hipoteses

- FTP: o codigo usava sonda UDP a 1.1.1.1 para descobrir IP, ocultava algumas etapas/errno e dependia de std::thread que pode encerrar um build sem excecoes quando falta recurso. Esses pontos foram removidos/reforcados. Sem Result/errno/etapa do Lite, nao se prova a causa fisica exclusiva; Wi-Fi, NIFM/BSD, recursos/applet, bind/listen e threads continuam distinguiveis pelo novo diagnostico na tela.
- Download: o codigo anterior fazia tres tentativas imediatas e apagava o parcial em falha. Isso e causa comprovada da ausencia de recuperacao persistente naquele fluxo. O SDK documenta SSL bloqueante com timeout padrao de cinco minutos; downloads agora usam modo nao bloqueante/espera limitada, sem alterar verificacao de certificados.
- Versao: os NROs anteriores validados continham os NACP esperados, mas o numero nao mudou no relato fisico. Caminho/copia antiga/instalacao continuam hipoteses. A candidata usa identidade derivada dos fontes e caminho na UI, alem de NACP e inventario somente leitura, sem declarar a causa do aparelho descoberta.

## Implementacao

FTP local NIFM/gethostid, posse compartilhada preservada, pthread com retorno verificado, etapas/codigos por origem e contexto de applet/memoria/donos na tela. [FTP](FTP.md).

DownloadManager e adaptador HTTP/OAuth com area por conta/tarefa, checkpoints/CRC, revisao remota, Range/If-Range, pausa/continuacao/cancelamento, backoff limitado e recuperacao explicita. Arquivos concluidos/parentes nao sao apagados quando outro item falha. [Contrato e limites](DOWNLOADS_DRIVE.md).

Build identity em version.h/NRO/UI/manifesto e scanner limitado somente leitura. validate_nro/package exigem identidade dos fontes, alem da versao NACP. [Instalacao](VERSAO_E_INSTALACAO.md).

## Execucoes por etapa

- Rede: sete testes de ciclo de vida, compilacao nativa e suite com NRO da etapa: **108/108 aprovados**.
- Gerenciador/identidade intermediarios: cinco testes de build/scanner, 16 de downloads e 50 de rede/Drive; compilacao nativa de 894.680 bytes e suite com NRO: **133/133 aprovados**.
- Refinamento: 18 cenarios de gerenciador, incluindo rede completamente indisponivel apos o corpo e destino concorrente, mais controlador I/O com timeout/abort/certificado.
- Final: **138/138 testes aprovados com NRO novo, zero reprovados/ignorados**. A regressao final confirma tambem que erro de protocolo do download nao desconecta nem invalida a conta e que falha de thread de cliente aparece no diagnostico e permite nova conexao.

## Resultado final da candidata

| Verificacao | Resultado desta execucao |
| --- | --- |
| Suite anterior ao build | 138 casos, 136 aprovados e dois dependentes de NRO explicitamente ignorados; 68,231 s |
| Suite com NRO real | 138 aprovados, zero falhas/ignorados; 68,547 s |
| Compilacao | devkitA64 r29.2-1/libnx 4.12.0-1, via /workspace/sfm-environment/build.sh em copia descartavel |
| NRO/NACP | 898.776 bytes, versao 0.10.11, titulo Switch File Manager; segmentos/ASET/NACP aprovados |
| Build incorporado | v0.10.11-77bda6d17361, correspondente aos fontes, ao manifesto e a inspecao do NRO |
| Icone | JPEG oficial preservado e comparado byte a byte no NRO |
| Pacote | ZIP/lista fechada, exclusao de segredos ficticios e sidecars SHA-256 aprovados |
| Documentacao | 29 arquivos ativos: links/ancoras/imagens validos; Markdownlint em 28 arquivos, zero erros |
| Workflows | actionlint 1.7.7 aprovado; automacao existente executa os novos testes e publica dist |
| Git | diff --check aprovado; build nao reescreveu fontes |

Imagem fixada por digest:

```text
devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282
```

Hashes da compilacao local final:

```text
SwitchFileManager.nro — 898776 bytes
89bf4ffd9d518e770e77ce0befec8ec3cfc3ad7e324a3d1839a6150063a6c5bc

assets/icon.jpg
858ebcb92444859e77ba2485a9532bedda25254ddc00d3fca72517eb5ff38cca
```

Avisos normais restantes sao de funcoes estaticas nao usadas em stb_truetype e suporte a grandes arquivos em miniz; nenhum erro de compilacao. Builds remotos podem ter hash diferente por caminhos de debug, mas devem manter a mesma identidade de fontes; conferir o sidecar do artefato especifico.

A regressao de armazenamento detectou inicialmente falta de recuperacao antes do primeiro journal; a correcao preserva candidatos anteriores e inicia outro parcial reconhecido sem promover conteudo nao verificado. As suites completas foram repetidas apos as correcoes, sem reutilizar aprovacao antiga.

## Arquivos principais

| Area | Arquivos alterados/adicionados |
| --- | --- |
| FTP/rede | socket_runtime.cpp/.h, wifi_transfer.cpp/.h, network/io_wait.h |
| Downloads | cloud/download_manager.cpp/.h, gdrive.cpp/.h, credential_store.cpp/.h (journal Transfer) |
| UI/identidade | app/main.cpp, diagnostics/build_info.cpp/.h |
| Build/distribuicao | VERSION, scripts/prepare.py, validate_nro.py, package.py |
| Testes | fronteiras host e testes Python de downloads, HTTP, rede, build/scanner, I/O e pacote |
| Documentacao | README, CHANGELOG, guias, relatorios historicos, notas v0.10.11 e checklist |

Logo oficial e terceiros preservados; nenhuma credencial real, arquivo pessoal ou projeto separado Switch-Manager foi alterado. O fluxo de login interativo/persistencia aprovado foi preservado, com regressoes automatizadas.

## GitHub e limites

O [PR #1](https://github.com/TemudjinBR/switch-file-manager/pull/1) registra os commits e a execucao exata do [Actions](https://github.com/TemudjinBR/switch-file-manager/actions/workflows/build.yml), com artefato experimental, NRO/ZIP/checksums. Sem merge, force-push, alteracao direta de main ou Release.

Nao houve nova validacao fisica. NIFM, modo applet/memoria completa, TLS real, FAT32/exFAT, latencia/perda de energia e controles/toque exigem o novo checklist. Backups/parciais ficam preservados e consomem espaco; nao ha retomada de upload nem fila persistente de arquivos ainda nao iniciados de uma arvore. Prazos DNS/TCP dependem do sistema.
