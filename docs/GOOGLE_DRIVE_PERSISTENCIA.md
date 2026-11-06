# Persistência OAuth e contas — 0.10.10 experimental

## Base preservada e causa ainda limitada

O relato da v0.10.8 ocorreu após autorização no Android. A Missão 3 demonstrou um defeito condicional: `rename(temporario, token.txt)` sobre destino existente funciona no host POSIX, mas fsdev/libnx pode recusar com EEXIST. Não existe errno do console que prove essa como a chamada exata do relato físico. O [relatório histórico da Missão 3](historico/VALIDACAO_MISSAO_3.md) preserva sua validação.

O SDK utilizado é libnx 4.12.0-1/devkitA64 r29.2-1. Na [fonte oficial](https://github.com/switchbrew/libnx/blob/7644c9b26099aa2d2145bc72a21ee24190e92085/nx/source/runtime/devices/fs_dev.c), rename delega a fsFsRenameFile, fsync chama fsFileFlush e lstat é alias de stat. Não se ignora falha de sincronização nem se atribui o problema genericamente a fsync inexistente.

## Contrato da camada de armazenamento

`src/cloud/credential_store.cpp`:

1. Verifica cada mkdir e confirma que EEXIST corresponde a diretório.
2. Recupera o backup reconhecido mais recente se o canônico estiver ausente; temporários nunca são promovidos automaticamente a login.
3. Lê o anterior. Conteúdo idêntico exige somente releitura, sem nova escrita.
4. Descobre slots no diretório e cria candidato exclusivo com O_EXCL.
5. Trata escritas parciais/EINTR, rejeita zero bytes e verifica fsync/close.
6. Relê/compara candidato; move anterior para backup livre e candidato para canônico vazio.
7. Relê/compara o canônico; em falha tenta isolar candidato e restaurar anterior, registrando também falha de recuperação.

Há limite de conteúdo de 64 KiB, mas **não existe mais o limite artificial de 128 slots**. A numeração descoberta usa 64 bits e continua após 127; um overflow extremo é diagnosticado, sem apagar cópias. Backups `.sfm-old-NNN`, candidatos `.sfm-new-NNN` e temporários legados `.tmp.*` são preservados nas gravações normais. Eles ainda consomem espaço: não existe promessa de armazenamento infinito nem expurgo automático de credenciais.

O mutex protege chamadas internas, sem bloquear FTP/outros processos. As duas renomeações não são atomicidade contra corte de energia. FAT32/exFAT usam o serviço de arquivos do Switch; os testes POSIX com semântica de rename imposta não simulam controlador, corrupção, cache ou energia do SD.

## Estado de conta em arquivo único

`drive-account.json`, no diretório legado, reúne esquema, chave aleatória da autorização, client ID, refresh token, nome/identificador opcionais, preferência keep, enabled e forgotten. Isso evita uma transação dividida entre token e preferência. O arquivo novo tem precedência; `google.json`, `token.txt` e `login.disabled` não são apagados nem migrados destrutivamente na importação.

Se ainda não houver estado novo, o aplicativo lê os legados. O token adotado fica vinculado à nova autorização para permitir limpeza seletiva futura. Um estado novo inválido gera erro SD e é preservado; não se retorna silenciosamente a um token legado potencialmente de outra conta.

`storeAccount` usa a camada verificada e executa outra releitura antes de publicar a sessão. Se essa confirmação falhar, tenta restaurar o estado anterior persistido; falhas da restauração são registradas separadamente. Nunca se confirma a nova conta pela mera existência do candidato.

## Preferência, desconexão e troca

- Manter conectado é persistido no mesmo registro. Desativar conserva a autorização, mas impede restauração na próxima abertura. Reconectar é explícito.
- Desconectar limpa sessão em memória e grava enabled=false. Não apaga autorização. Se a gravação falhar, a UI avisa; a autorização anterior pode voltar após reinício.
- Ao autorizar outra conta, o aplicativo conserva uma cópia da sessão anterior em memória e pausa chamadas remotas. A nova autorização exige refresh próprio e fica pendente até salvar/reler. Cancelar descarta o candidato volátil e restaura a sessão anterior quando válida.
- Depois do commit, os painéis Drive antigos têm listagens, marcações, filtros e pilhas limpos antes de abrir a raiz nova. A fila existente é local; não há fila remota de outra conta preservada indevidamente.
- Falhas Google, rede, SD e invalid_grant não apagam credenciais. Renovação preserva refresh anterior quando a resposta não o rotaciona. O client ID do registro precisa corresponder à credencial atual.

Se o aplicativo fechar antes de salvar um candidato, essa autorização volátil se perde. Falhas SD múltiplas, corrupção e acesso externo simultâneo podem impedir restauração; o diagnóstico distingue falha original e de recuperação, sem promessa de resolver qualquer defeito físico.

## Esquecimento explícito e seletivo

Somente a ação **Esquecer esta conta**, com confirmação na UI, chama eraseMatching. Não existe limpeza automática de tokens por erro de rede ou por limite de slots.

1. Limpa a sessão e grava um marcador forgotten sem refresh, bloqueando restauração inclusive do legado.
2. Examina registros/cópias reconhecidos. Atribui-os pela chave da autorização ou identificador de conta confirmado pela API no mesmo client ID.
3. Persiste impressões SHA-256 dos tokens identificados no marcador, antes de removê-los. Isso permite retomar exclusão parcial sem gravar novamente o segredo removido.
4. Remove registros com tokens da conta e tokens legados cujo conteúdo corresponda às impressões conhecidas, incluindo temporários atribuíveis.
5. Preserva outras contas identificadas, arquivos pessoais e google.json compartilhado. Conteúdo parcial/desconhecido fica preservado e é informado como resíduo não atribuível, sem alegar limpeza completa.

Falha de unlink é registrada e permite tentar Esquecer novamente. O marcador sem token permanece. Exclusão é lógica no filesystem: não se promete apagamento forense de setores flash, nem revogação remota Google. Não há chamadas de exclusão de arquivos no Drive nessa ação.

## OAuth e diagnóstico

O nonce state e o verificador PKCE usam aleatoriedade do sistema; produção usa CSRNG/libnx e SHA-256 do SDK. No host, getrandom e implementação SHA-256 de teste são conferidos contra hashlib e vetor RFC 7636. O desafio é S256; formulários POST exigem nonce local, e a URL de retorno deve carregar o state atual. Esses dados não entram em logs ou artefatos.

Categoria DRIVE_SD em `sdmc:/switch/Switch File Manager/logs/sfm.log`: arquivo lógico, etapa e errno, sem valores de credenciais. Famílias: criar-diretorio, listar-transacoes, abrir/gravar/sincronizar/fechar-temporario, ler/comparar, mover-anterior, instalar-novo, restaurar-anterior, confirmar conta e apagar-autenticacao. Result da inicialização de sockets pertence à categoria REDE e não é errno.

HTTP OAuth não bem-sucedido mostra status e código permitido, sem ecoar corpo/error_description. Nome/identificador da conta, tokens, código OAuth, state, verifier e client secret não são registrados. Estado e backups continuam segredos em texto no SD; não há criptografia prometida.

Testes e resultados: [TESTES.md](TESTES.md) e [RELATORIO_VALIDACAO.md](RELATORIO_VALIDACAO.md). Aprovação do hardware permanece pendente.
