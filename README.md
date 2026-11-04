# Switch File Manager

Gerenciador de arquivos homebrew para Nintendo Switch, incluindo Switch Lite com Atmosphère. Desenvolvido em C++17 com libnx, com interface de dois painéis no estilo Commander.

## Versão atual: v0.8.0 — fila de transferências

**Estado:** atualizações até v0.7.0 confirmadas em testes pelo proprietário; a v0.8.0 foi preparada para compilação e teste, mas ainda não há confirmação de teste.

## Funcionalidades disponíveis

- Navegação por arquivos e pastas em dois painéis.
- Copiar, mover, criar pastas, renomear e excluir arquivos.
- Seleção múltipla: marcar todos, somente arquivos, somente pastas, inverter e limpar.
- Verificação de cópias locais antes de excluir originais em movimentações.
- Progresso, contador de arquivos, velocidade média e tempo restante estimado.
- Cancelamento pelo botão B e tentativa de limpeza de arquivos incompletos.
- Fila temporária de até 20 tarefas locais, executadas sequencialmente.
- Base de integração Google Drive e ZIP, sujeita a testes específicos.
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
