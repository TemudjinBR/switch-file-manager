# Referência do Switch File Manager

Documento de entrada para uma pessoa ou IA continuar o projeto. Leia este arquivo, `ARQUITETURA.md`, `TESTES.md` e `PROBLEMAS_CONHECIDOS.md` antes de editar. A organização segue o propósito da referência do Omni36OS, adaptado ao Switch e à libnx.

## Identidade e invariantes

- Repositório: https://github.com/TemudjinBR/switch-file-manager. Branch de integração: `main`.
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
| `src/network/wifi_transfer.cpp` | FTP, sessões, sockets de dados, contadores e eventos |
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
| `sdmc:/switch/switch-manager/token.txt` | Token de renovação |
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
