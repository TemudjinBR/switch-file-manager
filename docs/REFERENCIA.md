# Referência do Switch File Manager

Documento de entrada para uma pessoa ou IA continuar o projeto. O [índice de documentação](README.md) separa guias de uso, inventário e evidências de cada missão. Leia este arquivo, `ARQUITETURA.md`, `TESTES.md` e `PROBLEMAS_CONHECIDOS.md` antes de editar. A organização segue o propósito da referência do Omni36OS, adaptado ao Switch e à libnx.

## Identidade e invariantes

- Repositório: [TemudjinBR/switch-file-manager](https://github.com/TemudjinBR/switch-file-manager). Branch de integração: `main`.
- Aplicativo C++17 para Nintendo Switch desbloqueado/Atmosphère; Switch Lite é alvo prioritário.
- Nome do binário: `SwitchFileManager.nro`. Título NACP: `Switch File Manager`.
- Preservar Commander com dois painéis, FTP, Drive, ZIP, toque e controles físicos.
- Não recriar o aplicativo, não alterar o projeto separado Switch-Manager, não apagar configurações ou dados de usuários.
- Documentação e relatórios novos em português brasileiro. Código de terceiros mantém seus avisos originais.
- Desenvolvimento em branch própria e PR. Não usar force-push na `main` nem publicar Release por simples push de código.

## Mapa para localizar mudanças

| Local | Responsabilidade |
| --- | --- |
| `src/app/main.cpp` | Inicialização, modelo de painéis, modais, controles, fila e coordenação das ações |
| `src/ui/renderer.cpp`, `include/ui/renderer.h` | Framebuffer, desenho, fontes e cache de glifos; mesma paleta e dimensões originais |
| `src/core/paths.cpp`, `include/core/paths.h` | Caminhos comuns e constantes de compatibilidade |
| `src/storage/filesystem.cpp` | Consulta de arquivos, listagem com erros, tamanho e nomes disponíveis |
| `src/storage/operations.cpp` | Cópia exclusiva, comparação integral, remoção e notificações de progresso independentes da UI |
| `src/network/socket_runtime.cpp` | Posse compartilhada de sockets por FTP/Drive, Result e retry |
| `src/security/oauth.cpp` | CSRNG/PKCE S256 em produção e fronteira de teste |
| `src/network/wifi_transfer.cpp` | FTP, sessões, sockets de dados, contadores e eventos |
| `src/cloud/credential_store.cpp`, `include/cloud/credential_store.h` | Persistência OAuth, diagnóstico de E/S, backup e recuperação; sem exclusão automática |
| `src/cloud/gdrive.cpp` | HTTPS libnx, parser JSON, OAuth, API Drive e servidor local de login |
| `src/archives/zipx.cpp` | ZIP com miniz, progresso, cancelamento e CRC |
| `src/diagnostics/logger.cpp` | Criação de diretórios, gravação, rotação e diagnóstico de falhas de log |
| `src/security/path_policy.cpp` | Nomes FAT/exFAT, pastas críticas e componentes seguros de ZIP |
| `include/` | Contratos dos módulos; sem headers gerados versionados |
| `assets/icon.jpg` | Ícone incorporado ao NRO |
| `third_party/` | miniz e stb_truetype preservados, com licenças nos próprios fontes |
| `scripts/` | Versão, preparação, compilação, validação NRO e distribuição |
| `tests/` | Testes Python, executáveis C++ no host e stub libnx limitado ao FTP de teste |
| `docs/legacy/` | Materiais históricos; Makefile e workflow daqui são inativos |
| `release-notes/` | Notas revisadas por versão |
| `.github/workflows/` | CI e publicação separados |
| `VERSION` | Única fonte de versão, formato X.Y.Z |

## Dados persistentes: não migrar implicitamente

| Caminho no Switch | Conteúdo |
| --- | --- |
| `sdmc:/switch/switch-manager/google.json` | Credencial OAuth individual |
| `sdmc:/switch/switch-manager/token.txt` | Token de renovação legado, preservado |
| Mesmo diretório, `drive-account.json` | Conta/refresh, preferência/estado e marcador sem token após esquecer |
| Mesmo diretório, `login.disabled` | Estado legado lido na importação não destrutiva |
| Mesmo diretório, `*.sfm-old-NNN` e `*.sfm-new-NNN` | Backups e candidatos com segredos; preservar e não publicar |
| `sdmc:/switch/switch-manager/favoritos.txt` | Favoritos |
| `sdmc:/switch/switch-manager/historico.txt` | Histórico de navegação |
| `sdmc:/switch/Switch File Manager/logs/sfm.log` | Log atual |
| Mesmo diretório, `sfm.log.1` | Rotação anterior |
| `sdmc:/switch/SwitchFileManager/logs/` | Local antigo dos logs; não é apagado nem movido |

Nunca colocar esses dados em commits, artefatos ou relatórios. Os testes usam valores fictícios em diretórios temporários. Alterar o nome do aplicativo não autoriza trocar o caminho de dados legado.

## Rotina de trabalho

1. Conferir `git status`, branch, HEAD, CI, Releases e estado real antes de presumir falhas antigas.
2. Alterar apenas o módulo necessário. Evitar voltar a inserir patches de C++ no Makefile.
3. Executar `python3 tests/run.py`. Após cada alteração estrutural, compilar com devkitPro e validar o NRO.
4. Usar `bash scripts/build.sh` no ambiente preparado. Não é necessário um computador local.
5. Conferir `git diff --check` e se o build não alterou fontes. Os avisos restantes dos terceiros estão documentados.
6. Registrar limitações físicas sem apresentá-las como teste concluído. Atualizar changelog, documentação e notas se houver mudança de versão.
7. Commit, push da branch e PR com evidências. Sem merge automático nesta missão.

Para uma versão futura, editar `VERSION` e criar `release-notes/vX.Y.Z.md`. O cabeçalho `build/generated/version.h` é produzido automaticamente; não editá-lo nem versioná-lo.

## CI e erro histórico 128

`build.yml` executa testes no host e depois compila no contêiner devkitPro fixado por digest. O checkout desativa persistência de credenciais. O shell do contêiner configura `safe.directory` somente para `$GITHUB_WORKSPACE` e verifica o repositório com `git -C`.

O erro histórico acontecia em `git config user.name` após a compilação, ao tentar fazer commit automático. O checkout adicionava confiança em um HOME temporário, sem garantir a mesma configuração no shell seguinte. A diferença de propriedade reproduziu exatamente o erro 128; a confiança explícita corrigiu a reprodução. Não usar `safe.directory '*'`. O build atual não cria commits, tags ou Releases.

`release.yml` exige execução manual, tag existente, `VERSION` correspondente, notas e `docs/validacao-console/vX.Y.Z.md`. A confirmação física sem relatório não atende ao gate. O workflow recompila/testa a revisão imutável e recusa Release existente. A automação não consegue comprovar a veracidade do relatório físico: a revisão humana continua necessária.

## Estado ao encerrar a reorganização

A auditoria inicial é de 05/11/2026, base `db81e91ff73852a933f8c93a5d4177c3a98de2f0`. As migrações foram incorporadas antes da mudança de pastas. A UI ainda coordena painéis e operações no arquivo principal; a separação de todos os controladores fica para mudanças pequenas futuras. Não apresentar esta etapa como eliminação de todo o acoplamento.

Resultados reproduzíveis: `docs/RELATORIO_VALIDACAO.md`, logs de Actions e artefatos da branch/PR. Testes reais de SSL Google, SD FAT32/exFAT, controles e estabilidade prolongada continuam pendentes no Switch.

## Continuidade após a Missão 2

- Identidade oficial: `assets/icon.jpg`, 256×256, bytes preservados no README e no NRO; regras em [assets/branding](../assets/branding/README.md). Não gerar/substituir a marca.
- O catálogo da UI e os limites têm evidência em [FUNCIONALIDADES.md](FUNCIONALIDADES.md), com todos os submenus; os guias de controles, armazenamento, FTP e ZIP complementam o Drive.
- A fila continua local, até 20 tarefas e não persistente. Favoritos/histórico são persistentes; ordenação, filtro, marcações e painéis são estados de sessão.
- Há relato do proprietário de falha física na v0.10.8 no Switch Lite: **“Falha ao salvar login no cartão SD.”**. A causa exata da persistência não está confirmada; não atribuir a rejeição ao Google nem anunciar OAuth corrigido. A Missão 2 não altera esse fluxo.
- Não há capturas autênticas disponíveis nesta missão. `docs/screenshots/README.md` orienta sua futura inclusão sem criar links para arquivos inexistentes.
- Resultados anteriores: [validação da Missão 1](historico/VALIDACAO_MISSAO_1.md). Resultados atuais: [relatório de validação](RELATORIO_VALIDACAO.md). As 37 aprovações antigas não devem ser reutilizadas como teste de uma revisão nova.
- Descrição/tópicos administrativos: conferir o estado observado e a limitação de acesso em [MISSAO_2.md](MISSAO_2.md). Alterar README não equivale a alterar o campo Sobre.
- PR #1 permanece o local de trabalho; não fazer merge, force-push, publicação de Release ou modificação direta da main nesta missão.

## Continuidade após a Missão 3

A Missão 3 entregou a versão experimental **0.10.9**, no mesmo PR #1 e branch `codex/reorganizacao-estabilizacao`. Os trabalhos e relatórios das Missões 1/2 continuam preservados. Não houve alteração do logotipo, organização de pastas, FTP, ZIP ou dados pessoais.

O fluxo Drive só confirma login após gravar e reler o token. Erros de armazenamento têm origem SD, etapa e errno; falhas Google/rede não apagam credenciais. A troca de arquivo evita sobrescrita por rename, pois libnx difere do POSIX do host. Nova tentativa de salvar usa o candidato já recebido, enquanto ele estiver na memória. Consultar [GOOGLE_DRIVE_PERSISTENCIA.md](GOOGLE_DRIVE_PERSISTENCIA.md) antes de alterar o algoritmo.

A reprodução com EEXIST comprova uma incompatibilidade do código anterior; não identifica por si só a chamada que falhou no Lite do proprietário. A correção permanece candidata até validação física FAT32/exFAT, renovação e reinício. Resultados atuais em [RELATORIO_VALIDACAO.md](RELATORIO_VALIDACAO.md); os [resultados da Missão 2](historico/VALIDACAO_MISSAO_2.md) são históricos.

## Continuidade após a Missão 3.1

A Missao 3.1 entregou **0.10.10 experimental**, mesma branch/PR #1. Antes de alterar autenticação, ler o [contrato de contas/persistência](GOOGLE_DRIVE_PERSISTENCIA.md). O arquivo drive-account.json é a autoridade quando existe; nunca retornar silenciosamente ao token legado se o estado novo for inválido/forgotten. Desconectar preserva autorização; Esquecer é explícito, seletivo e local. PKCE/state não tornam a combinação Android↔Switch automaticamente homologada pelo Google.

FTP/Drive usam exclusivamente socket_runtime; não inserir socketInitialize/socketExit nos consumidores. O último dono encerra, e falhas permitem retry. VERSION prevalece sobre argumentos antigos do make; NACP é conferido por conteúdo. Sobre, Versão e diagnósticos e log de caminho ajudam a diferenciar cópias antigas no Homebrew Menu.

O [histórico da Missão 3](historico/VALIDACAO_MISSAO_3.md) preserva sua execução anterior; [relatório atual](RELATORIO_VALIDACAO.md) e logs/artefatos do Actions são a evidência desta revisão. Esse era o estado daquela rodada; o anexo posterior aprova FTP isolado/sequencial e acesso/persistência/reconexão Drive, sem aprovar simultaneidade ou a nova candidata. Não fazer merge ou Release nesta missão.

## Continuidade da Missao 3.2

Atual: 0.10.12 experimental, mesma branch/PR. O relato fisico aprovou acesso/persistencia/reconexao do Drive na 0.10.10, tambem aprovou FTP isolado/antes/depois do Drive; reprovou FTP simultaneo com Drive, versao e retomada de download; a 3.1 nao esta aprovada para producao. Preservar a autenticacao e os dados reais.

DownloadManager esta em src/cloud/download_manager.cpp, contrato em include/cloud/download_manager.h. Usa Transport para reaproveitar HTTP/OAuth existentes, estado por conta/tarefa e journals transfer.bin no diretorio tecnico downloads. Nunca voltar a remover destino/parcial em falha de rede. Conhecer [DOWNLOADS_DRIVE.md](DOWNLOADS_DRIVE.md) antes de alterar checkpoints, cancelamento ou publicacao.

Diagnostico local FTP usa NIFM/gethostid e codigos por etapa; nenhuma sonda externa e necessaria. Versao/build sao gerados em prepare; nao editar version.h. build_info le NROs sem altera-los e tem limites de busca. Resultados atuais no [relatorio](RELATORIO_VALIDACAO.md); [3.1 historica](historico/VALIDACAO_MISSAO_3_1.md).

A rodada 0.10.12 parte de c78ce35 (não voltar à base d66994c descartando correções). O servidor FTP existente foi preservado; a investigação de sessões BSD/esperas e o callback UI são descritos em ARQUITETURA e FTP. Testes de simultaneidade exigem barreira no mesmo processo: testes sequenciais de posse não bastam. Downloads mantêm SFMDL2 compatível, CRC, consulta de espaço e escolha de reinício. Não prometer retomada de upload.

No callback setNetworkPump, somente UI/FTP/controle de cancelamento; nunca chamar outra API Drive. Estados de autenticação pertencem à thread principal. O SDK/SSL real e o modo applet não são simulados pelos stubs. A versão do pacote é 0.10.12; instalação Android e comparação versão/build/caminho em VERSAO_E_INSTALACAO. Relatos físicos correntes em VALIDACAO_SWITCH_LITE_3_2.

## Atualizador — Missão 3.3

Base da Missão 3.3: **0.10.13 experimental**, preservando 7116e34 e o PR #1; candidata de interface atual: 0.10.14, descrita abaixo. Ler [ATUALIZACOES](ATUALIZACOES.md) e [PROMOCAO_E_RELEASES](PROMOCAO_E_RELEASES.md) antes de mudar atualização/distribuição. O alvo é apenas o caminho oficial validado contra argv[0]/NACP/build. Nunca instalar pelo processo principal, baixar de Actions com token pessoal, mover tags ou promover por CI verde. main foi observada sem proteção; gate bloqueia até configuração administrativa real e homologação. Não inventar relatório físico aprovado.

## Continuação na Missão 3.6

Candidata atual de interface: **0.10.14 experimental**, branch `codex/menu-global-sobre-0-10-14`, derivada do PR #2/02d6b06; PR #1/84e3f76 preservado. Não misturar artefatos 0.10.13 com esta correção. [Relatório](MISSAO_3_6.md), [controles](CONTROLES.md), [mapa antes/depois](MENU_CONTEXTUAL.md), [instalação Android](VERSAO_E_INSTALACAO.md). + Menu global, X contextual, − saída confirmada; Sobre na quinta entrada.

`include/ui/navigation.h` contém a política de entrada e rodapé compartilhada com testes; `src/ui/about_view.cpp` desenha o Sobre. `scripts/generate_about_icon.py` deriva RGB888 do JPEG oficial com Pillow apenas na preparação de arte; header rastreado permite compilar sem Pillow. Não regenerar QR/beneficiário. O renderer tem backend HOST_TEST para prévia com fonte substituta; não é captura do Switch nem teste de latência/memória no aparelho.
