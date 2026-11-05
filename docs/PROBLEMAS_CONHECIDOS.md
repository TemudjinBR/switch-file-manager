# Problemas conhecidos e limites

## Dependem do Switch físico

- Esta reorganização não foi executada em Nintendo Switch. Não há aprovação de interface, controles, toque, fonte do sistema, abertura de outros NROs ou modo applet.
- Logs e gravação atômica precisam de confirmação no SD com caminhos contendo espaços, FAT32 e exFAT, cartão cheio e retomada após falhas. Testes POSIX no host não reproduzem drivers do console.
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
