# Google Drive e OAuth

## Estado do recurso

> **Importante:** nesta versão, cada usuário precisa criar credenciais OAuth próprias no Google Cloud. O recurso ainda é experimental. Não compartilhe credenciais nem códigos de autorização.

## Falha de persistência relatada no Switch Lite

**Problema aberto da v0.10.8:** o proprietário relatou no Nintendo Switch Lite, durante a autenticação, a mensagem **“Falha ao salvar login no cartão SD.”**. Esse relato foi fornecido na especificação da Missão 2; esta execução não repetiu o teste fisicamente e não recebeu os parâmetros completos de firmware/Atmosphère/formato do SD ou um errno do ponto de falha.

No código, `tokenRequest` em [gdrive.cpp](../src/cloud/gdrive.cpp) emite essa mensagem quando falha a gravação local do token de renovação por `writeFile`, após receber e interpretar uma resposta de token. A sequência usa criação de temporário, escrita, flush, fsync, fechamento e rename. **Não foi identificado qual desses passos falha no console.** O texto não comprova rejeição da autorização pelo Google, nem autoriza atribuir a causa a uma chamada específica sem diagnóstico.

Os testes com servidor simulado e filesystem do host não reproduzem automaticamente o comportamento do SD/libnx. Portanto o fluxo permanece **experimental e com falha física conhecida**, mesmo com testes automatizados aprovados. A Missão 2 só documenta esse problema; não altera OAuth, credenciais, tokens ou o C++ para tentar corrigi-lo.

A correção específica deverá instrumentar os pontos de persistência sem registrar valores de token e testar com dados de uma conta de teste no console, preservando os arquivos anteriores. Não apagar o diretório legado ou compartilhar tokens como procedimento de diagnóstico.

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
8. Aguarde o fechamento automático da tela de login no Switch.

**Segurança:** o endereço de retorno pode conter um código de autorização. Nunca o compartilhe. A página local usa HTTP; utilize apenas rede Wi-Fi confiável. Não ignore avisos de segurança de um aplicativo desconhecido: confira se o projeto OAuth foi criado por você. O fluxo atual precisa ser revisado e modernizado antes de uma distribuição pública definitiva.

### 3. Local dos arquivos de configuração

O caminho legado continua sendo:

- `sdmc:/switch/switch-manager/google.json` — credencial OAuth.
- `sdmc:/switch/switch-manager/token.txt` — token de renovação.

A pasta `switch-manager` é o caminho ainda utilizado pelo código, apesar do nome atual **Switch File Manager**. Não compartilhe esses arquivos nem os envie ao GitHub.

### 4. Limitações atuais

- A falha de persistência relatada acima pode impedir a conclusão do login no Switch Lite; o fluxo não é considerado estável.
- Navegação e transferências têm testes com API simulada, sem aprovação integral em conta Google/SSL reais.
- **Mover** entre Drive e SD está bloqueado por segurança: use **Copiar**, confira os arquivos e só depois exclua a origem manualmente.
- Copiar diretamente de uma pasta do Drive para outra não é suportado.
- Arquivos nativos Google Docs e Planilhas não são exportados nesta versão.
- ZIP diretamente no Drive não é suportado.

### 5. Solução de problemas

- **Página não abre no celular:** confira a mesma rede Wi-Fi, o IP mostrado no Switch e a porta `8080`.
- **“Falha ao salvar login no cartão SD.”:** é a falha local aberta descrita acima; a causa exata ainda não está confirmada. Não concluir que foi uma recusa do Google e não apagar configurações como tentativa automática de correção.
- **Google recusa explicitamente o login:** confira a Drive API, o tipo de credencial **Aplicativo para computador**, a tela de consentimento e os usuários de teste.
- **Código inválido:** inicie uma nova autorização; códigos não podem ser reutilizados.
- **Drive não carrega:** confira conexão com a internet, data e hora e mensagens de erro exibidas no Switch.
