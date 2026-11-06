# Histórico de alterações

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
