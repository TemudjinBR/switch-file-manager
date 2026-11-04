# Switch File Manager

Gerenciador de arquivos homebrew para Nintendo Switch, incluindo Switch Lite com Atmosphère. Desenvolvido em C++17 com libnx, com interface de dois painéis no estilo Commander.

## Versão em preparação: v0.10.0 — pré-verificação de downloads do Google Drive

**Estado:** v0.7.0 confirmada em teste pelo proprietário. A v0.8.0 ainda não teve confirmação explícita de teste; a v0.9.0 aguarda compilação e teste no Switch Lite.

## Funcionalidades disponíveis

- Navegação por arquivos e pastas em dois painéis.
- Copiar, mover, criar pastas, renomear e excluir arquivos.
- Seleção múltipla: marcar todos, somente arquivos, somente pastas, inverter e limpar.
- Verificação de cópias locais antes de excluir originais em movimentações.
- Progresso, contador de arquivos, velocidade média e tempo restante estimado.
- Cancelamento pelo botão B e tentativa de limpeza de arquivos incompletos.
- Fila temporária de até 20 tarefas locais, executadas sequencialmente.
- Google Drive: navegação e transferência herdadas da base; na v0.9.0, pré-varredura recursiva de downloads, contagem de arquivos e interrupção explícita em erros de listagem ou profundidade.
- Base de ZIP, sujeita a testes específicos.
- Compilação automatizada no GitHub Actions, gerando `SwitchFileManager.nro`.

## Histórico de versões

| Versão | Implementação | Situação informada |
| --- | --- | --- |
| v0.1.0 | Estrutura inicial, Makefile e GitHub Actions | Compilação inicial confirmada |
| v0.2.0 | Novo ícone e melhorias visuais | Teste confirmado |
| v0.3.0 | Seleção múltipla avançada | Teste confirmado |
| v0.4.0 | Proteções em cópia, exclusão e sobrescrita | Teste confirmado |
| v0.5.0 | Verificação antes de excluir origem em movimentações locais | Teste confirmado |
| v0.6.0 | Progresso, velocidade, tempo restante e cancelamento | Teste confirmado |
| v0.7.0 | Segurança de gravação, cancelamento e limpeza de parciais | Teste confirmado |
| v0.8.0 | Fila de até 20 operações locais | Teste pendente de confirmação |
| v0.9.0 | Pré-varredura de pastas do Drive, contagem de arquivos, checagem de profundidade e erros ao criar diretórios | Compilação e teste pendentes |

## Compilação

O workflow `.github/workflows/build.yml` compila o projeto com devkitA64/libnx. Depois de uma execução bem-sucedida, baixe o artefato `SwitchFileManager.nro` na aba **Actions** do GitHub.

## Limitações atuais

- A fila v0.8.0 é temporária e limitada inicialmente a operações locais no SD.
- Operações Google Drive não participam da fila. Autenticação, upload e download ainda exigem testes específicos.
- Movimentações entre Google Drive e SD permanecem restritas por segurança.
- USB OTG (FAT/FAT32/exFAT/NTFS), transferência Wi-Fi e instalação NSP/NSZ/XCI ainda não estão implementados ou validados.
- Teste operações destrutivas somente com arquivos descartáveis; mantenha backups.
- Nunca envie tokens ou credenciais Google ao repositório.

## Próximas versões planejadas

- **v0.9.0:** melhorias no Google Drive, navegação e downloads de pastas.
- **v0.10.0:** transferência de arquivos pela rede local com proteção de acesso.
- **Posteriores:** avaliar USB OTG, formatos de disco, instalação de pacotes e testes de estabilidade.

## Projeto

- Repositório: https://github.com/TemudjinBR/switch-file-manager
- Idioma: português brasileiro.
- Plataforma: Nintendo Switch com homebrew compatível.

## v0.10.0 — transferência Wi-Fi local (experimental)

- Menu **X > Ações > Transferência Wi-Fi**: inicia ou interrompe servidor HTTP local na porta 8765.
- No Android, abrir o endereço exibido no Switch, conectado à mesma rede Wi-Fi.
- Envio de vários arquivos (um por vez) pelo navegador e download dos arquivos listados.
- Pasta compartilhada exclusiva: `sdmc:/switch/SwitchFileManager/WiFi`. Não expõe o restante do cartão SD.
- Código de acesso aleatório incluído no endereço, renovado a cada inicialização; nenhum acesso externo à internet é necessário.
- Arquivos recebidos usam nome temporário `.sfm-part`, são sincronizados e renomeados após gravação completa.
- Não sobrescreve arquivos existentes; limite de 1 GiB por upload nesta primeira implementação.
- **Limitações:** sem criptografia HTTPS, sem barra de progresso no Switch, sem suporte a pastas inteiras, sem retomada, sem transferência em segundo plano ao fechar o aplicativo. Use apenas rede Wi-Fi confiável.
- **Validação:** código preparado para teste; compilação com devkitA64 e teste físico pendentes.

### Histórico de versões

| Versão | Alteração |
| --- | --- |
| v0.10.0 | Servidor Wi-Fi local com envio e download via navegador Android |
