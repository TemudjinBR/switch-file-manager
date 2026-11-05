# Switch File Manager

Gerenciador de arquivos homebrew para Nintendo Switch, com interface de dois painéis.

**Versão atual: v0.10.7 — experimental.**

## Download e instalação

1. Abra [Releases](https://github.com/TemudjinBR/switch-file-manager/releases).
2. Baixe `SwitchFileManager.nro` e coloque-o em uma pasta de `sdmc:/switch/` no cartão microSD.
3. Inicie o aplicativo pelo Homebrew Menu.

## Como configurar o Google Drive

> **Importante:** nesta versão, cada usuário precisa criar credenciais OAuth próprias no Google Cloud. O recurso ainda é experimental. Não compartilhe credenciais nem códigos de autorização.

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

O código da v0.10.7 usa:

- `sdmc:/switch/switch-manager/google.json` — credencial OAuth.
- `sdmc:/switch/switch-manager/token.txt` — token de renovação.

A pasta `switch-manager` é o caminho ainda utilizado pelo código, apesar do nome atual **Switch File Manager**. Não compartilhe esses arquivos nem os envie ao GitHub.

### 4. Limitações atuais

- A navegação e as transferências entre Drive e SD estão presentes no código, mas a integração ainda precisa de testes adicionais.
- **Mover** entre Drive e SD está bloqueado por segurança: use **Copiar**, confira os arquivos e só depois exclua a origem manualmente.
- Copiar diretamente de uma pasta do Drive para outra não é suportado.
- Arquivos nativos Google Docs e Planilhas não são exportados nesta versão.
- ZIP diretamente no Drive não é suportado.

### 5. Solução de problemas

- **Página não abre no celular:** confira a mesma rede Wi-Fi, o IP mostrado no Switch e a porta `8080`.
- **Google recusa o login:** confira a Drive API, o tipo de credencial **Aplicativo para computador**, a tela de consentimento e os usuários de teste.
- **Código inválido:** inicie uma nova autorização; códigos não podem ser reutilizados.
- **Drive não carrega:** confira conexão com a internet, data e hora e mensagens de erro exibidas no Switch.

## FTP e logs

O FTP existe, mas a estabilidade de transferências grandes ainda está sendo validada. O código da v0.10.7 prevê logs em `sdmc:/switch/SwitchFileManager/logs/sfm.log`, porém a criação desse arquivo no console ainda não foi confirmada. Revise os logs antes de compartilhá-los.

## Estado do desenvolvimento

- GitHub Releases: publicação configurada.
- Google Drive: integração experimental, exige OAuth individual.
- FTP e logs: testes de estabilidade pendentes.
- Auto-update, USB OTG e demais funções planejadas: pendentes.
