# Auditoria inicial e decisões

Data: 05/11/2026. Repositório auditado: `TemudjinBR/switch-file-manager`, branch `main`, HEAD inicial `db81e91ff73852a933f8c93a5d4177c3a98de2f0`. O checkout inicial estava limpo. O projeto separado Switch-Manager não foi acessado ou modificado.

## Inventário anterior às mudanças

O repositório tinha 30 arquivos versionados, sem suíte de testes e sem documentação central para continuidade. O código próprio estava na raiz: `main.cpp` (2040 linhas), `gdrive.cpp` (1119), `wifi_transfer.cpp` (527), `zipx.cpp` (246), seus headers e logger inline de 25 linhas. miniz e stb_truetype também ficavam na raiz, junto do ícone.

Foram examinados os módulos próprios, contratos, bibliotecas incorporadas, scripts de atualização, Makefile efetivo, workflow em `.github/workflows`, workflow duplicado na raiz, Makefile.txt, README e documentos antigos, ZIP legado, histórico Git e metadados/etapas/logs de Actions e Releases. Terceiros foram inventariados e preservados sem atualização; isso não equivale a uma auditoria formal de segurança de toda a biblioteca miniz/stb.

A referência Omni36OS (revisão `9034fc49952705906c3cf185d27adcd48fc1855f`) foi lida especialmente em `docs/REFERENCIA.md`, organização de fontes/documentos e rotina de testes. Foram adotados princípios de manutenção e continuidade, sem copiar código ou regras de instalação específicas do R36S.

## Estado real de cada área

| Área | Achado inicial e encaminhamento |
| --- | --- |
| C++17/libnx | Makefile válido para devkitPro, sem RTTI/exceções. Compilação inicial real passou: NRO de 775.896 bytes. Não havia falha de compilação a presumir |
| Interface | Commander de dois painéis, framebuffer e fonte do sistema; toque e controles preservados. Extraído o renderer, mantida coordenação dos painéis na aplicação |
| Arquivos/pastas | Cópia, fallback de mover com comparação e exclusão existentes. Extraídos e testados; criação exclusiva e limpeza protegida reforçadas |
| FTP | Servidor ativo/passivo com até quatro clientes, REST de download, temporários e contadores. Migrações de refresh/status/buffers/logs existiam somente no build; incorporadas e testadas localmente |
| Google Drive/OAuth | API e SSL nativo presentes; dados em `sdmc:/switch/switch-manager`. HOST_TEST já existia, mas não havia testes versionados. Fluxo OAuth manual continua experimental |
| Downloads Drive | Testes novos reproduziram falso sucesso em HTTP 206 incompleto e 416; corrigidos tamanho, Content-Range e finalização. Sem completar arquivo artificialmente |
| Renovação de autorização | Teste reproduziu listagem vazia após HTTP 401 por concatenação de respostas JSON. O buffer é reiniciado ao receber novos headers |
| Persistência OAuth | Erros de salvar credencial/token eram ignorados. Escrita temporária com sincronização e substituição; falha agora informada e dados anteriores preservados |
| ZIP | miniz já suportava as operações. Teste com `/dev/full` reproduziu falso sucesso no fechamento da extração; corrigido. Travessia de caminhos e cancelamento testados |
| Logs | Escrita inline ignorava falhas e não tinha rotação. Caminho novo existia em patch, sem ativação estável no código. Substituído por módulo verificável, preservando diretórios antigos |
| Preparação | Cinco scripts eram executados por `make`; script de logs era ativado dinamicamente pelo workflow. `apply_ftp_update.py` dependia do comentário de versão e falhava em repetição após as demais migrações |
| Versão | Makefile 0.10.7, comentário principal 0.10.0 e workflow convertendo para 0.10.8. Unificados em VERSION; fontes já representam o que será compilado |
| Documentação/duplicados | Material antigo conflitante foi movido para `docs/legacy`; apenas Makefile e workflows ativos são instruções de build atuais |

## Histórico de Actions e Release

A única Release encontrada foi `v0.10.7`, com `SwitchFileManager.nro` e checksum. Não foi modificada.

Execuções recentes inspecionadas:

| Execução | Resultado observado |
| --- | --- |
| [42030224625](https://github.com/TemudjinBR/switch-file-manager/actions/runs/42030224625), HEAD db81e91 | Build e upload passaram; persistência de versão/notas falhou com 128 |
| 42028403924, HEAD 3bdeabc | Falha no workflow de publicação |
| 42026385807, HEAD f9b4015 | Falha no workflow de publicação |
| 42023791823 e execuções anteriores recentes | Há sucessos; portanto o projeto já compilava antes desta missão |

O log da execução 42030224625 mostra checkout com `safe.directory` em HOME temporário, compilação concluída e falha em `git config user.name` no passo de persistência. A reprodução no mesmo contêiner, com repositório de outro UID, retornou exatamente `fatal: not in a git directory` e código 128. Após configurar confiança somente em `/repo`, o comando retornou 0.

Isso sustenta a diferença de propriedade/confiança do checkout como causa do erro histórico, sem atribuí-lo a erro de C++, NRO ou falta de `.git` após uma compilação. O workflow atual configura explicitamente o workspace seguro no shell e não tenta fazer commits durante o build.

## Estratégia executada

1. Registrar o estado limpo e compilar a base numa cópia descartável com o script devkitPro fornecido.
2. Aplicar as migrações uma vez no checkout, registrar o resultado e retirar os patches do caminho de compilação. Testar versão/preparação e recompilar.
3. Mover módulos, headers, terceiros e material histórico, atualizar Makefile, testar e recompilar.
4. Extrair renderer, caminhos, segurança, armazenamento e logger, testar no host e recompilar.
5. Criar testes de protocolo/integração; reproduzir e corrigir falhas de integridade e finalização; ampliar regressões.
6. Consolidar documentação e workflows, validar NRO/distribuição, repetir build incremental e publicar commits em branch com PR.

Cada resultado efetivamente executado é detalhado no relatório de validação. Itens físicos não foram marcados como aprovados. Nenhuma Release foi publicada nesta missão.
