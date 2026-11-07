# Histórico de alterações

## 0.10.14 — experimental, Missão 3.6 (07/11/2026)

- **+** Menu global com cinco categorias fixas; X exclusivamente contextual; − Sair com confirmação, cancelamento cooperativo e encerramento normal dos serviços.
- Rodapé físico/tátil compartilhado, seleção preservada ao abrir menus, SD↔Drive em Arquivos e pastas; ZL+Y/ZL+X preservados.
- Sobre diretamente na raiz, sem duplicação em Sistema; removido atalho redundante para configurações do atualizador. Mover remoto deixa de ser oferecido quando não é suportado.
- Ícone do Sobre derivado do JPEG oficial, RGB888 256×256 nativo, blit direto; JPEG NACP e matriz Pix originais preservados. Apoio voluntário abaixo das informações.
- Diagnóstico de cópias mostra SHA-256 somente por leitura; ferramenta de inspeção cobre ocultos e extensão NRO em maiúsculas, sem exclusão automática.
- Novos testes executáveis de atalhos/contexto/toque/saída durante cópia, renderização real em host e fidelidade de ícone/QR. Instalação Android documentada; homologação no Lite pendente.

## 0.10.13 — experimental, Missão 3.3 (06/11/2026)

- Atualizações com canal estável padrão, experimental explícito, consulta em worker, preferências, notas, tamanho/progresso e confirmação de download/instalação.
- Manifesto público oficial por HTTPS, SHA-256 incremental, validação NRO/NACP/build e caminho oficial; nenhum token pessoal embutido.
- Auxiliar separado instala após encerrar o principal, com parcial exclusivo, backup, journal, recuperação e rollback; falhas não autorizam sobrescrever dados desconhecidos.
- Terceira posse de sockets para o atualizador, sem encerrar donos FTP/Drive.
- Workflows manuais de pré-Release e promoção: revisão, proteção real, CI e homologação com commit/hashes exatos; sem publicação automática por CI verde ou reutilização de versão/canal.
- Pacote com dois NROs, notas e checksums; testes de falhas/transação/canais e documentação de confiança/limites. Nenhuma homologação física, merge ou Release nesta missão.

## 0.10.12 — experimental, simultaneidade e recuperação (06/11/2026)

- Matriz física corrigida pelo novo anexo: FTP isolado/antes/depois do Drive aprovados; simultaneidade, retomada e versão exibida reprovadas na 0.10.10. Preservado o trabalho posterior de c78ce35/0.10.11.
- Orçamento de oito sessões BSD, fallback diagnosticado para três somente em erros explícitos de recursos; buffers padrão preservados.
- FTP usa sockets não bloqueantes e espera fora do IPC BSD. Cada worker fecha seus próprios descritores, evitando shutdown pela UI sobre números reutilizados.
- Navegação, envio e download Drive consultam a UI durante espera TCP/TLS; Y inicia/para FTP, B cancela. TLS mantém verificação de certificados. DNS e E/S SD ainda dependem do sistema.
- Encerramento Drive em requisição pendente adia liberação do SSL/sockets até fechar a conexão, preservando a posse FTP. Estado OAuth legado e persistência aprovados preservados.
- Downloads verificam espaço para bytes restantes/margem, aceleram CRC compatível com journals existentes e oferecem reinício seguro com preservação do parcial anterior.
- Testes no mesmo processo sobrepõem navegação/download/upload Drive e até quatro transferências FTP, com barreiras e comparação de conteúdo; regressões de ENOSPC, consentimento e prefixo esparso de 1,2 GiB em arquivo de 2 GiB.
- Identidade/NACP/pacote 0.10.12, instalação pelo Android e novo checklist físico. Sem merge/main/Release; não homologado no Switch Lite.

## 0.10.11 — experimental, Missao 3.2 (06/11/2026)

- Relato inicialmente recebido foi incorporado; o anexo posterior retifica FTP: isolado e sequencial funcionaram, simultaneo falhou. Versao e recuperacao de download falharam. A 3.1 nao esta aprovada para producao.
- FTP usa IP local NIFM/BSD, sem sondagem externa, com diagnosticos de etapa/Result/errno/pthread na tela e threads com retorno verificado.
- Downloads passam a ter area tecnica, checkpoints/CRC, Range/If-Range, revisao remota, backoff limitado, pausa/continuacao/cancelamento e recuperacao explicita apos reinicio.
- Mudanca de versao remota ou Range recusado reinicia em outro parcial; destinos pessoais/concluidos sao preservados. Nenhuma retomada de upload implementada.
- SSL dos downloads usa operacoes nao bloqueantes, com limite de espera sem ignorar erros de certificado.
- Versao/build visiveis no cabecalho e diagnostico de caminho/copias do NRO somente leitura; pacote exige identidade correspondente aos fontes.
- Autenticacao aprovada, dados legados, logo oficial e estrutura preservados. Sem merge/main/Release.

## 0.10.10 — experimental, Missão 3.1 (05/11/2026)

- FTP e Drive compartilham inicialização/encerramento de sockets, com posse idempotente, retry e diagnóstico Result/contexto de applet/memória.
- VERSION autoritativo mesmo com override antigo; NACP conferido por conteúdo, versão visível no menu Ações e caminho em log/manifesto/instruções.
- Menu de contas: Entrar com Google, Manter conectado, Reconectar conta salva, Desconectar conta, Trocar de conta e Esquecer esta conta.
- Troca só publica sessão após persistência/releitura; cancelamento restaura sessão anterior válida e sucesso limpa listagens dos painéis.
- PKCE S256, state e nonce dos formulários; status de conexão e identificação opcional sem segredos nos logs.
- Estado único em drive-account.json com precedência sobre legados preservados. Esquecimento local exige confirmação, identifica cópias e informa resíduos; não revoga no Google.
- Slots descobertos em 64 bits removem o limite artificial de 128 sem expurgo automático.
- Testes ampliados com rede compartilhada, contas fictícias, falhas SD, recuperação, exclusão parcial, PKCE, NACP, duplicados e distribuição sem segredos.
- Causas físicas de FTP/versão ainda exigem confirmação pelo Result/caminho no Switch Lite; sem merge ou Release.

## 0.10.9 — experimental, persistência OAuth (05/11/2026)

- Persistência separada com mkdir verificado, escrita parcial/EINTR, sincronização, fechamento e releitura.
- Substituição compatível com rename sem sobrescrita do libnx; backup, rollback e recuperação preservam arquivos anteriores.
- Login confirmado somente depois de gravar/reler; nova tentativa local do candidato já autorizado.
- Falhas SD identificam etapa/errno; respostas Google e erros de rede são distintos, sem registrar segredos.
- Erros OAuth, inclusive invalid_grant, não removem tokens. Desconexão usa marcador e preserva token/credencial.
- Testes com dados fictícios, falhas de armazenamento, semântica fsdev, renovação e primeiro login.
- A falha com destino existente foi reproduzida no host; a causa exata do relato no Lite e a validação física permanecem pendentes. Sem Release.

## 0.10.8 — base das Missões 1/2 (05/11/2026)

- Fontes separados em aplicação, núcleo, interface, armazenamento, rede, nuvem, arquivos compactados, diagnósticos e segurança.
- miniz e stb_truetype mantidos em `third_party`; ícone em `assets`; materiais antigos preservados em `docs/legacy` e notas em `release-notes`.
- Migrações antes executadas no `make` incorporadas ao código. Preparação idempotente gera somente cabeçalho de versão.
- `VERSION` unifica versão do NACP, pacote e log de inicialização.
- Logs em `sdmc:/switch/Switch File Manager/logs/`, com rotação de aproximadamente 1 MiB, proteção entre threads e retorno de erros.
- Correção de downloads Drive incompletos, HTTP 416 e intervalos de bytes inconsistentes; destino existente preservado.
- Resposta JSON reiniciada ao renovar autorização após HTTP 401, evitando listagem vazia por mistura de respostas.
- Salvamento OAuth em arquivo temporário com substituição somente após gravação bem-sucedida; falhas são informadas. Caminho legado preservado.
- Cópia local cria destino exclusivo; limpeza de cópia falha não apaga destino preexistente. Erros de listagem são propagados nas operações locais.
- Extração ZIP detecta falhas de flush e fechamento; sanitização mantém caracteres UTF-8 completos.
- Testes automatizados no host para arquivos, logs, FTP, API Drive simulada, versão, preparação, estrutura, NRO e distribuição.
- CI para branches e PRs, imagem devkitPro fixada por digest, confiança Git restrita ao workspace e remoção de commits automáticos na `main`.
- Publicação manual separada, com tag existente, correspondência de versão, notas, relatório no console e nova compilação/testes.

### Apresentação e documentação — Missão 2

- README com a marca oficial intacta, introdução, benefícios e catálogo completo de 12 categorias, sem reduzir o aplicativo a poucas funções.
- Guias de funcionalidades, controles, armazenamento, FTP e ZIP; índice de documentação e regras para capturas reais.
- Registro do relato de falha de persistência do login Drive no Switch Lite; causa exata não confirmada e nenhuma correção OAuth nesta missão.
- Validação adicional de links/âncoras/imagens, inventário de menus e preservação do ícone do NRO.
- Relatório da Missão 1 preservado em histórico; resultados novos separados por execução.
- Sem novas funções do aplicativo, sem troca de ícone e sem alteração de versão. Metadados administrativos só são considerados aplicados quando confirmados no GitHub.

Esta versão não foi publicada como Release nesta auditoria e ainda requer validação física.

## 0.10.7 — Release pública anterior

Tag e Release já existentes no início da auditoria. O build aplicava patches Python para ativar diagnósticos e otimizações FTP; os fontes versionados não representavam integralmente o código compilado. A Release contém NRO e SHA-256. Os workflows seguintes compilaram, mas falharam na etapa de persistência Git.

## Notas anteriores

- [v0.10.3](release-notes/CHANGELOG-v0.10.3.md)
- [v0.10.2](release-notes/CHANGELOG-v0.10.2.md)

O histórico Git é a fonte para versões antigas sem notas individuais. Arquivos de atualização anteriores permanecem em `docs/legacy/` e não orientam a compilação atual.
