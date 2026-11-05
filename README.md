# Switch File Manager

Gerenciador de arquivos homebrew em C++17 e libnx para Nintendo Switch, com interface Commander de dois painéis. Destinado a consoles desbloqueados com Atmosphère, incluindo Switch Lite. Ferramentas e serviços usados pelo projeto têm opções gratuitas.

**Versão em desenvolvimento: 0.10.8, experimental.** A Release pública anterior é a [v0.10.7](https://github.com/TemudjinBR/switch-file-manager/releases/tag/v0.10.7). Compilação e testes no host não comprovam funcionamento no console.

## Recursos e estado

| Recurso | Situação |
| --- | --- |
| Dois painéis, seleção múltipla, ordenação, busca, favoritos e histórico | Implementados; interface e controles dependem de regressão no console |
| Copiar, mover, renomear, criar e apagar no SD | Implementados; cópia, comparação e remoção têm testes no host |
| Conferência integral antes de apagar a origem no fallback de mover | Implementada; comparação byte a byte, sem hash criptográfico |
| Servidor FTP na porta 2121, modos ativo e passivo | Implementado; integração testada com sockets locais, sem validação de desempenho no Switch |
| Google Drive: login, pastas, download, upload, renomeação e lixeira | Experimental; testes com API simulada, sem autorização Google real nesta auditoria |
| ZIP: compactar, inspecionar, extrair e cancelar | Implementado; testes reais com miniz no host |
| Fila de transferências e abertura de outros NROs | Implementadas na aplicação; validação no console pendente |
| Logs persistentes com rotação e indicação de erro | Implementados; armazenamento e concorrência testados no host |
| USB OTG, atualização automática e novos provedores | Planejados; não implementados nesta reorganização |

Mover entre SD e Drive está bloqueado por segurança. Drive para Drive, exportação de documentos nativos Google e ZIP remoto não são suportados.

## Instalação e uso

1. Baixe um pacote validado em [Releases](https://github.com/TemudjinBR/switch-file-manager/releases). Artefatos de [Actions](https://github.com/TemudjinBR/switch-file-manager/actions) são versões de teste.
2. Extraia a pasta `switch` na raiz do microSD, ou coloque `SwitchFileManager.nro` em `sdmc:/switch/Switch File Manager/`.
3. Inicie pelo Homebrew Menu. Compare o SHA-256 antes de instalar.

| Controle | Ação |
| --- | --- |
| A / B | Abrir / subir pasta |
| X / Y | Ações / marcar item |
| L ou R; esquerda ou direita | Alternar painel |
| ZL / ZR | Subir / descer uma página |
| Direcional ou analógico esquerdo | Navegar |
| − / + | Google Drive / sair |
| Toque | Selecionar, abrir, rolar e usar botões da interface |

O FTP permite acesso anônimo de leitura e escrita ao SD inteiro, após confirmação na interface. Use a rede confiável indicada no aplicativo e desligue o servidor ao terminar.

[Configurar Google Drive](docs/GOOGLE_DRIVE.md). Credenciais, tokens, favoritos e histórico continuam em `sdmc:/switch/switch-manager/`. Essa compatibilidade é somente um caminho de dados; o projeto separado **Switch-Manager** não foi modificado. Logs novos: `sdmc:/switch/Switch File Manager/logs/sfm.log`; logs antigos permanecem onde estavam.

## Compilação e testes

Requisitos: Python 3, GCC/G++ para testes no host, GNU Make e devkitPro com devkitA64/libnx para produzir o NRO. Não há dependências Python externas para a suíte principal.

```bash
python3 tests/run.py
bash scripts/build.sh
```

O segundo comando usa devkitPro disponível no ambiente ou `/workspace/sfm-environment/build.sh` quando executado no ambiente preparado. Também é possível compilar diretamente:

```bash
export DEVKITPRO=/opt/devkitpro
export DEVKITA64="$DEVKITPRO/devkitA64"
export PATH="$PATH:$DEVKITA64/bin:$DEVKITPRO/tools/bin"
make clean
make -j2
python3 scripts/validate_nro.py SwitchFileManager.nro
python3 tests/run.py --nro SwitchFileManager.nro
python3 scripts/package.py
```

A distribuição fica em `dist/`. O build gera arquivos de versão em `build/generated/` e não modifica fontes. `VERSION` é a única fonte de versão do Makefile, log inicial, NACP e pacote.

## Documentação para manutenção

- [Referência para retomar o trabalho](docs/REFERENCIA.md)
- [Arquitetura e dependências](docs/ARQUITETURA.md)
- [Testes e critérios de publicação](docs/TESTES.md)
- [Auditoria inicial e correções](docs/AUDITORIA.md)
- [Problemas conhecidos](docs/PROBLEMAS_CONHECIDOS.md)
- [Roadmap](docs/ROADMAP.md) e [histórico](CHANGELOG.md)

Organização inspirada na [referência do Omni36OS](https://github.com/mpjsilva/omni36os/blob/main/docs/REFERENCIA.md): documentação para continuidade, módulos com responsabilidade real e verificações reproduzíveis. O código e o sistema operacional do Omni36OS não foram incorporados.
