# Persistência OAuth — candidata 0.10.9

## Evidência e limite da causa

O relato físico da v0.10.8 é: autorização aceita no navegador Android e, no aplicativo, “O Google recusou: Falha ao salvar login no cartão SD.”. O prefixo atribuía um erro local ao Google. Não há registro da chamada/errno daquele console; não é possível comprovar sua causa exata nesta execução.

Foi comprovado um defeito condicional do código anterior: `rename(temporario, token.txt)` pressupunha substituição de destino existente. Na implementação fsdev do libnx, essa operação é delegada ao serviço de arquivos e pode retornar EEXIST. Um executável do código anterior, com HTTP 200 fictício e wrapper que reproduz essa semântica, apresentou a mesma mensagem quando já existia token. Esse cenário também afeta credenciais atualizadas e tokens rotacionados; não explica necessariamente uma primeira gravação sem arquivo anterior.

Outras hipóteses físicas continuam abertas: criação de diretório, cartão cheio/somente leitura, erro de escrita, flush, fechamento, renomeação ou releitura. A v0.10.9 registra esses pontos separadamente. O Switch Lite e o cartão do proprietário não foram acessados nem submetidos a teste.

## POSIX, libnx, FAT32 e exFAT

O ambiente efetivamente usado contém **libnx 4.12.0-1** e **devkitA64 r29.2-1**. A fonte oficial correspondente é [fs_dev.c, revisão 7644c9b](https://github.com/switchbrew/libnx/blob/7644c9b26099aa2d2145bc72a21ee24190e92085/nx/source/runtime/devices/fs_dev.c):

- `fsdev_rename` chama `fsFsRenameFile`; o mapeamento de resultado inclui EEXIST. O host POSIX normalmente substitui um arquivo de destino, ocultando a incompatibilidade em testes comuns.
- `fsdev_fsync` chama `fsFileFlush`. A falha não foi atribuída genericamente a “fsync ausente”. O código não ignora ENOSYS/EINVAL ou outros erros de sincronização.
- `lstat` é associado a `fsdev_stat`, pois links simbólicos não são suportados. No host, os arquivos especiais/links são recusados nas verificações do armazenamento.

FAT32 e exFAT passam pelo serviço fsdev no Switch. Os pequenos arquivos OAuth não atingem o limite de arquivo do FAT32, mas os testes do host não representam driver, controlador, cache, corrupção ou perda elétrica desses formatos. Ambos exigem validação física separada; não se promete atomicidade de metadados nem proteção universal contra corte de energia.

## Contrato de armazenamento

`src/cloud/credential_store.cpp` mantém o caminho legado `sdmc:/switch/switch-manager/` e não usa remove/unlink. O fluxo é:

1. Criar cada diretório necessário, conferir EEXIST como diretório e propagar falhas.
2. Se o arquivo canônico sumiu, procurar o backup reservado mais recente; restaurar e reler. Candidatos temporários nunca são aceitos automaticamente como login válido.
3. Ler o anterior, quando existe. Se o conteúdo já é idêntico, não escrever novamente.
4. Reservar um slot posterior aos ocupados, criar candidato com O_EXCL, tratar escrita parcial/EINTR e rejeitar retorno zero.
5. Executar fsync, conferir close, reler candidato e comparar exatamente.
6. Mover o anterior para backup livre; mover o candidato para o caminho canônico vazio. Não depender de rename sobre destino existente.
7. Reler e comparar o canônico. Em falha, tentar isolar candidato e restaurar anterior; registrar também erro de recuperação, se houver.
8. No OAuth, executar outra releitura estrita antes de ativar acesso/renovação em memória e confirmar login.

Backup: `<arquivo>.sfm-old-NNN`. Candidato: `<arquivo>.sfm-new-NNN`. O armazenamento limita cada conteúdo a 64 KiB e reserva até 128 slots por arquivo. Nenhum backup/candidato antigo é apagado automaticamente. O limite produz `reservar-transacao`/ENOSPC; isso não prova que o SD inteiro esteja cheio. Arquivos antigos `.tmp.*` também são preservados.

A sequência é protegida por mutex dentro do módulo, mas não bloqueia acesso externo pelo FTP. As renomeações não formam uma transação atômica contra corte de energia. A recuperação cobre ausência do canônico com backup reconhecido; corrupção de cartão ou do canônico exige diagnóstico, sem substituição automática por uma credencial potencialmente obsoleta.

## Login, renovação e desconexão

O candidato recebido do Google só vira sessão confirmada após salvar/reler. Em falha local, ele permanece na memória e o navegador oferece **Tentar salvar login novamente**. A nova tentativa não pede outro token ao Google. Se o aplicativo for fechado antes de salvar, esse candidato volátil é perdido.

A renovação preserva o refresh token anterior quando a resposta não envia outro; uma autorização nova exige seu próprio refresh token para evitar reutilizar a conta antiga. Respostas inválidas e falhas de rede/Google, inclusive invalid_grant, não apagam token.txt/google.json.

Desconectar limpa a sessão em memória e grava `login.disabled` com `1`. Isso impede login automático sem apagar token. Um login novo confirmado altera o marcador para `0`. Ausência do marcador mantém compatibilidade com instalações anteriores; valor inválido é erro local. Falha ao gravar desconexão é informada na interface, pois o login antigo poderá voltar no próximo início.

## Diagnósticos sem credenciais

O caminho é `sdmc:/switch/Switch File Manager/logs/sfm.log`, categoria `DRIVE_SD`. Exemplo fictício:

```text
[DRIVE_SD] arquivo=token.txt etapa=instalar-novo errno=17
```

A mensagem local começa por `SD: etapa=...; errno=...`, para preservar o diagnóstico mesmo na linha curta da interface. Respostas de autorização HTTP não bem-sucedidas começam por `Google:` e incluem status/código OAuth de uma lista permitida; descrições remotas não são ecoadas. Falhas de transporte começam por `Rede:`. Não se registram corpo HTTP, token, código OAuth, client secret ou URL com parâmetros de autenticação.

| Etapa ou família | Operação |
| --- | --- |
| `criar-diretorio` | mkdir e confirmação do diretório existente |
| `examinar-*` | Consulta de tipo/existência |
| `reservar-transacao` | Capacidade de slots sem sobrescrita |
| `abrir-temporario`, `gravar-temporario` | Criação exclusiva e loop de escrita |
| `sincronizar-temporario`, `fechar-temporario` | fsync e close |
| `abrir-leitura-*`, `ler-*`, `fechar-leitura-*`, `comparar-*` | Releituras e comparação |
| `mover-anterior`, `instalar-novo` | Renomeações para destinos livres |
| `isolar-novo`, `restaurar-anterior`, `restaurar-recuperacao` | Rollback/recuperação |
| `validar-credencial`, `validar-token-salvo`, `validar-estado-login` | Conteúdo local inválido, preservado |

`errno=0` aparece apenas em eventos de sucesso, não como diagnóstico fictício de uma falha. Números errno são os do runtime que executou a chamada. Falhas do próprio logger não mudam o resultado de persistência e são tratadas pelo logger existente.

Tokens, credenciais, backups e temporários continuam segredos em texto no SD. `.gitignore` cobre os nomes gerados; o empacotador inclui somente arquivos explicitamente permitidos. Não compartilhar diretórios de configuração ou backups. Os testes usam apenas dados fictícios.

## Validação

A regressão usa E/S real em diretórios temporários, injeção de erros somente no host, wrapper de rename e servidor HTTP fictício. Cobre primeiro login, renovação, rotação, escrita parcial, EINTR, EIO, ENOSPC, EROFS, sincronização indisponível, fechamento, releituras, rollback, falha dupla, saída abrupta entre renomeações, recuperação e retry local. Não é teste no cartão do proprietário.

Resultados da compilação real, NRO/distribuição e Actions: [RELATORIO_VALIDACAO.md](RELATORIO_VALIDACAO.md). Matriz física pendente: [TESTES.md](TESTES.md). A v0.10.9 é candidata experimental, sem Release e sem declaração de login corrigido no hardware.
