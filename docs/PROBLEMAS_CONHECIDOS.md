# Problemas conhecidos e limites

## Falha relatada no Nintendo Switch Lite — Google Drive

Na v0.10.8, o proprietário relatou **“Falha ao salvar login no cartão SD.”** durante a autenticação. O ponto de código corresponde à persistência local do token. A causa exata e a chamada que falha no console não foram confirmadas; não atribuir a rejeição ao Google. Os testes de host aprovados não anulam esse relato. A v0.10.9 implementa uma correção experimental com diagnóstico por etapa/errno e recuperação. Foi reproduzida a falha ao substituir destino existente com a semântica de rename do libnx, mas ainda não foi comprovada a chamada exata que falhou naquele console; ver [Google Drive](GOOGLE_DRIVE.md#falha-de-persistência-relatada-no-switch-lite).

## Dependem do Switch físico

- A execução de desenvolvimento não teve acesso a Nintendo Switch físico. Há o relato de falha de login acima, recebido do proprietário; ele não constitui aprovação de interface, controles, toque, fonte do sistema, abertura de outros NROs ou modo applet.
- Logs, sincronização e recuperação de gravação precisam de confirmação no SD com caminhos contendo espaços, FAT32 e exFAT, cartão cheio e retomada após falhas. Testes POSIX no host não reproduzem drivers do console.
- FTP exige validação de desempenho, memória, clientes simultâneos, rede móvel instável e parada durante transferências grandes. Os buffers de 128 KiB e sockets de 256 KiB não comprovam ganho no hardware.
- Google Drive usa SSL nativo não exercitado no host. Data/hora, certificados, permissões Google e limites reais da API precisam de teste.
- ZIP64 e arquivos maiores que 4 GiB não foram validados. FAT32 limita cada arquivo a menos de 4 GiB; o aviso de grandes arquivos emitido por miniz permanece registrado.
- Cancelamento, suspensão, Home e perda de energia podem deixar arquivos parciais. Nenhum teste automatizado daqui comprova atomicidade contra falha elétrica.

## Restrições implementadas

- Mover SD ↔ Drive está bloqueado. Cópia Drive → Drive, exportação de Docs/Planilhas e ZIP remoto não são suportados.
- O fluxo OAuth atual é experimental, exige credenciais individuais e cópia manual do código de retorno. O servidor local usa HTTP na porta 8080; não há modernização completa com PKCE nesta etapa.
- FTP é anônimo e sem TLS, com acesso ao SD inteiro. A confirmação já existente na UI deve ser mantida. Avisos de pastas críticas na UI não constituem bloqueio global de acesso pelo FTP.
- A retomada de upload FTP é recusada; REST é usado para downloads. Temporários preexistentes não são apagados automaticamente.
- Upload resumível Drive ainda aceita respostas de progresso sem conferir completamente o offset confirmado pelo Google. O teste local cobre o caminho básico; retomada real continua pendente.
- A integração cloud conserva HTTP, JSON e OAuth no mesmo arquivo. A aplicação ainda coordena painéis/modais/fila; a modularização não eliminou todo o acoplamento.

## Limites das garantias

- Verificação byte a byte antes da exclusão de origem não bloqueia alterações simultâneas pelo FTP ou outro processo; não é uma transação.
- Proteção de extração ZIP valida componentes do arquivo, sem sandbox de sistemas de arquivos arbitrários com symlinks preexistentes. O alvo normal é SD FAT/exFAT; não anunciar a mesma garantia para qualquer filesystem do host.
- Algumas consultas usadas para estatísticas/UI continuam tratando uma listagem indisponível como vazia. As operações locais de cópia, conferência e exclusão propagam falhas de listagem.
- O logger devolve erro e informa stderr; ainda não existe uma tela de diagnóstico de falhas do próprio logger. Registros FTP podem conter nomes/caminhos de arquivos e IPs; revisar antes de compartilhar.
- Tokens continuam armazenados como texto no SD para compatibilidade. Não há promessa de armazenamento criptografado nesta etapa.
- O repositório não define uma licença geral do aplicativo. As licenças dos terceiros foram preservadas; esta reorganização não escolhe uma licença pelo autor.

## Limites de interface e persistência inventariados na Missão 2

- Ordenação, filtro, marcações, fila e sessão dos painéis não são persistidos. Reload limpa marcações; favoritas/histórico locais ficam salvos, mas seu caminho de escrita ainda não propaga todos os erros à interface.
- A fila limita 20 tarefas locais, não tem rollback por tarefa e não aceita Drive. Um item já copiado pode exigir revisão antes de repetir uma tarefa interrompida.
- Exclusão recursiva consulta o progresso para desenhar/ler B, mas não oferece parada imediata garantida em toda a travessia; não anunciar cancelamento universal de exclusão.
- O analisador exibe nove maiores itens imediatos. O cálculo de propriedades limita aprofundamento e pode omitir itens inacessíveis; datas remotas não são buscadas.
- Não há tela Sobre, versão no cabeçalho da UI, bateria/relógio, visualizador universal ou instalador de NSP/XCI/NSZ implementados.

## Limites da persistência OAuth na v0.10.9

- Renomeações são feitas apenas para nomes livres; backup e candidato permanecem no SD. A sequência não é atomicidade contra perda elétrica. Uma interrupção pode exigir recuperar o anterior no próximo carregamento.
- Cada arquivo tem até 128 slots de transação e 64 KiB de conteúdo. O limite é diagnosticado como `reservar-transacao`/ENOSPC, mesmo havendo espaço físico. Não há expurgo automático de credenciais antigas.
- Backups e temporários contêm segredos em texto como os arquivos legados. Não compartilhar, versionar ou incluir em artefatos. O acesso simultâneo do FTP a esses arquivos não é bloqueado pela transação.
- Um candidato ainda não salvo permanece só na memória; fechar o aplicativo nessa condição perde a autorização recebida. O botão de nova tentativa evita repetir OAuth enquanto a sessão continua aberta.
- Erro ao persistir a desconexão é informado; o token permanece no SD e poderá ser reutilizado na próxima execução se o marcador não foi salvo.

Ver [contrato e evidências](GOOGLE_DRIVE_PERSISTENCIA.md).
