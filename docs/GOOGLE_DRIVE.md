# Google Drive e OAuth

## Estado do recurso

> **Importante:** nesta versão, cada usuário precisa criar credenciais OAuth próprias no Google Cloud. O recurso ainda é experimental. Não compartilhe credenciais nem códigos de autorização.

## Falha de persistência relatada no Switch Lite

**Relato da v0.10.8:** o proprietário conseguiu autorizar pelo navegador Android, mas o Switch Lite mostrou “O Google recusou: Falha ao salvar login no cartão SD.” e voltou ao login. Não foi recebido um errno da operação no hardware.

A **0.10.9 experimental** implementa uma correção candidata: diretórios verificados, temporário exclusivo, escritas parciais/EINTR, sincronização, fechamento, releitura, substituição sem sobrescrever por rename e backup/rollback/recuperação. O erro local agora é identificado como **SD**, separado de respostas **Google** ou falhas de **Rede**. A causa exata no aparelho ainda não foi confirmada e não há aprovação física do novo NRO.

O defeito de compatibilidade comprovado é a tentativa antiga de renomear temporário sobre `token.txt` existente: POSIX no host aceita, mas fsdev da libnx não oferece essa substituição. O teste com essa semântica reproduziu a falha antiga e passou com o novo algoritmo. O SDK desta execução, libnx 4.12.0, tem fsync ligado a `fsFileFlush`; portanto não se presume ausência genérica de sincronização.

Os testes também injetam disco cheio, somente leitura, falhas de abertura, escrita, sincronização, fechamento, releitura, instalação e rollback, sem valores reais. Consulte a [investigação completa](GOOGLE_DRIVE_PERSISTENCIA.md) e o [relatório atual](RELATORIO_VALIDACAO.md). Os registros anteriores da Missão 2 permanecem no histórico.

### Quando a autorização chegou, mas a gravação falhou

A página mostra **SD**, a etapa e o errno, mantém o login não confirmado e oferece **Tentar salvar login novamente**. O botão repete somente a persistência/verificação local da autorização mantida em memória; não reutiliza o código OAuth já consumido nem exige nova chamada de token ao Google. Se o aplicativo for encerrado antes de salvar, essa autorização em memória se perde e pode ser necessária uma nova autorização; os arquivos anteriores e cópias de recuperação permanecem preservados.

Não apagar ou formatar o diretório legado como diagnóstico. Não compartilhar o diretório completo, tokens, códigos ou credenciais.

## Capacidades implementadas, ainda experimentais

- Configuração individual de credencial e login por formulário local no celular.
- Renovação de autorização e tentativa de usar login salvo.
- Navegação de pastas e listagem paginada da API.
- Download de arquivos e árvores de pastas para SD, com preflight e limite de profundidade no fluxo de download.
- Upload de arquivos/pastas locais; arquivos usam o mecanismo resumível da API, com limitações de recuperação/offset ainda documentadas.
- Criação de pastas, renomeação e envio de itens à lixeira.
- Retorno de um painel ao SD e troca/desconexão da conta mediante confirmação. Retornar ao SD não é desconectar.

A listagem atual obtém id, nome, tipo e tamanho; não obtém data de modificação remota. O diálogo de propriedades não calcula tamanho total de pastas Drive. Favoritos e fila pertencem ao SD, sem integração remota nessa revisão.

## Configuração no fluxo existente

### 1. Criar credenciais OAuth

1. Acesse [Google Cloud Console](https://console.cloud.google.com/), crie ou selecione um projeto.
2. Habilite a **Google Drive API** em APIs e serviços.
3. Configure a tela de consentimento OAuth (Google Auth Platform). Caso use o modo de teste, adicione sua conta aos usuários de teste.
4. Crie um **ID do cliente OAuth** do tipo **Aplicativo para computador**.
5. Baixe o arquivo `client_secret...json` e guarde-o em segurança no celular ou computador.

Documentação: [OAuth para aplicativos instalados](https://developers.google.com/identity/protocols/oauth2/native-app).

### 2. Conectar o Switch pelo celular

1. Conecte Switch e celular à **mesma rede Wi-Fi**.
2. No Switch File Manager, escolha o painel e abra a opção **Google Drive**.
3. O Switch mostrará um endereço local, por exemplo `http://192.168.1.20:8080`. Use o **endereço exato** exibido na tela.
4. Abra esse endereço no navegador do celular.
5. Na primeira conexão, selecione o `client_secret...json` e toque em **Salvar credencial**.
6. Toque em **Entrar com o Google**, escolha sua conta e autorize o acesso solicitado.
7. A implementação atual pode redirecionar para `http://localhost` e mostrar uma página que não abre. Copie o **endereço completo** da barra do navegador, volte à página local do Switch, cole no campo do Passo 2 e toque em **Enviar para o Switch**.
8. O login só é confirmado após gravar e reler o token. Se houver erro **SD**, use **Tentar salvar login novamente**; aguarde o fechamento da tela no Switch somente depois da confirmação.

**Segurança:** o endereço de retorno pode conter um código de autorização. Nunca o compartilhe. A página local usa HTTP; utilize apenas rede Wi-Fi confiável. Não ignore avisos de segurança de um aplicativo desconhecido: confira se o projeto OAuth foi criado por você. O fluxo atual precisa ser revisado e modernizado antes de uma distribuição pública definitiva.

### 3. Local dos arquivos de configuração

O caminho legado continua sendo:

- `sdmc:/switch/switch-manager/google.json` — credencial OAuth.
- `sdmc:/switch/switch-manager/token.txt` — token de renovação.
- `login.disabled` no mesmo diretório — bloqueio persistente após desconexão, sem apagar token.
- Sufixos `.sfm-old-NNN` e `.sfm-new-NNN` — cópias de recuperação/temporários, que também podem conter segredos.

A pasta `switch-manager` é o caminho ainda utilizado pelo código, apesar do nome atual **Switch File Manager**. Não compartilhe esses arquivos nem os envie ao GitHub.

### 4. Limitações atuais

- A correção candidata da 0.10.9 ainda exige teste no Switch Lite/FAT32/exFAT. O fluxo continua experimental.
- Navegação e transferências têm testes com API simulada, sem aprovação integral em conta Google/SSL reais.
- **Mover** entre Drive e SD está bloqueado por segurança: use **Copiar**, confira os arquivos e só depois exclua a origem manualmente.
- Copiar diretamente de uma pasta do Drive para outra não é suportado.
- Arquivos nativos Google Docs e Planilhas não são exportados nesta versão.
- ZIP diretamente no Drive não é suportado.

### 5. Solução de problemas

- **Página não abre no celular:** confira a mesma rede Wi-Fi, o IP mostrado no Switch e a porta `8080`.
- **SD: etapa=...; errno=...:** falha local identificada. Use a repetição de salvamento oferecida; o detalhe permite localizar a chamada sem revelar o token. A causa do relato antigo no hardware ainda precisa ser confirmada.
- **Google: HTTP ...:** resposta efetiva do endpoint OAuth; dados locais são preservados inclusive em invalid_grant.
- **Rede:** falha de transporte/DNS/TLS, sem presumir rejeição do Google.
- **Google recusa explicitamente o login:** confira a Drive API, o tipo de credencial **Aplicativo para computador**, a tela de consentimento e os usuários de teste.
- **Código inválido:** inicie uma nova autorização; códigos não podem ser reutilizados.
- **Drive não carrega:** confira conexão com a internet, data e hora e mensagens de erro exibidas no Switch.
