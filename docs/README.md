# Documentação do Switch File Manager

A apresentação oficial está no [README principal](../README.md). A revisão do PR #1 usa **0.10.12 experimental**; a última Release pública verificada em 05/11/2026 é **v0.10.7**. Recursos implementados, resultados no host e validação física são classificados separadamente.

## Para usar o aplicativo

| Assunto | Documento |
| --- | --- |
| Catálogo e limites, com referência ao código | [Funcionalidades](FUNCIONALIDADES.md) |
| Botões, toque e contextos | [Controles](CONTROLES.md) |
| Arquivos locais, favoritos, histórico, fila e espaço | [Armazenamento](ARMAZENAMENTO.md) |
| Conta, configuração OAuth e falha de persistência no Lite | [Google Drive — experimental](GOOGLE_DRIVE.md) |
| Correção candidata, hipóteses, algoritmo e diagnósticos do SD | [Persistência do login Drive — Missão 3](GOOGLE_DRIVE_PERSISTENCIA.md) |
| Conexão local, comandos e limites | [FTP](FTP.md) |
| Compactação, extração, inspeção e cancelamento | [ZIP](ZIP.md) |
| Restrições e falhas abertas | [Problemas conhecidos](PROBLEMAS_CONHECIDOS.md) |
| Galeria futura, somente com capturas reais | [Guia de capturas](screenshots/README.md) |

## Para continuar o desenvolvimento

| Assunto | Documento |
| --- | --- |
| Entrada para pessoa ou IA retomar o projeto | [Referência](REFERENCIA.md) |
| Módulos e dependências | [Arquitetura](ARQUITETURA.md) |
| Comandos completos, cobertura e critérios de Release | [Testes](TESTES.md) |
| Evidências da revisão atual | [Relatório de validação](RELATORIO_VALIDACAO.md) |
| Base da reorganização e complemento da Missão 2 | [Auditoria](AUDITORIA.md) |
| Implementação futura, sem promessas de prazo | [Roadmap](ROADMAP.md) |
| Mudanças por versão | [Changelog](../CHANGELOG.md) e [notas 0.10.12](../release-notes/v0.10.12.md) |
| Marca e ícone que também entra no NRO | [Identidade oficial](../assets/branding/README.md) |
| Bibliotecas, créditos e licenças preservadas | [Terceiros](../third_party/README.md) |
| Histórico de apresentação e limitações externas da Missão 2 | [Relatório de entrega](MISSAO_2.md) |

Os relatórios das [Missão 1](historico/VALIDACAO_MISSAO_1.md) e [Missão 2](historico/VALIDACAO_MISSAO_2.md) ficam preservados como evidências anteriores; o relatório principal acompanha a execução atual.

## Documentação ativa e material histórico

`docs/legacy/` preserva instruções antigas, um Makefile duplicado inativo, um workflow de exemplo e o ZIP histórico v0.9.0. Consulte o [contexto desses arquivos](legacy/README.md), mas não os use para compilar a revisão atual.

`scripts/legacy/` mantém seis entradas Python de compatibilidade, ainda verificadas pelos testes. Elas só chamam a preparação atual, sem reaplicar patches ao C++. Não foram removidas nesta missão.

Para contribuir com documentação, verifique links e referências com `python3 scripts/check_docs.py` e rode a suíte em `python3 tests/run.py`. A revisão visual usa uma prévia em navegador do README; essa prévia não comprova funcionamento no Switch.

## Missão 3.1

- [Versão, NACP e cópias instaladas](VERSAO_E_INSTALACAO.md).
- [Gerenciamento de contas e OAuth](GOOGLE_DRIVE.md).
- [Ciclo compartilhado de sockets e falha FTP](FTP.md).
- [Validação anterior da Missão 3](historico/VALIDACAO_MISSAO_3.md).

O [relatório atual](RELATORIO_VALIDACAO.md) separa diagnóstico, implementação, automação e limites físicos.

## Missao 3.2

- [Downloads recuperaveis](DOWNLOADS_DRIVE.md).
- [Checklist fisico do Switch Lite](VALIDACAO_SWITCH_LITE_3_2.md).
- [Historico da Missao 3.1](historico/VALIDACAO_MISSAO_3_1.md).

Os resultados fisicos recebidos na Missao 3.2 impedem considerar a 3.1 aprovada para producao.
