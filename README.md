# Switch File Manager — base inicial v0.1.0

Projeto homebrew C++17/libnx para Nintendo Switch, incluindo Switch Lite com Atmosphère.

## Estado desta entrega

Esta é uma **base de migração**, não o produto final. O código funcional foi adaptado do pacote Switch Manager enviado pelo proprietário dos dois projetos. A arquitetura, as operações de arquivo, a interface e o Drive ainda derivam do projeto anterior. A interface Commander será redesenhada em versões posteriores.

Mudanças nesta etapa:
- Identidade do executável: `SwitchFileManager.nro`.
- Título do aplicativo: Switch File Manager.
- Makefile com lista explícita dos módulos a compilar.
- GitHub Actions: compila e entrega artefato de teste, **sem publicar release estável automaticamente**.

## Compilação

O workflow usa a imagem oficial `devkitpro/devkita64` e executa `make`. A compilação no GitHub e o funcionamento no Switch ainda **não foram verificados nesta entrega**.

## Limitações

- Google Drive pode continuar usando os caminhos `switch/switch-manager` herdados da base anterior. Não migre tokens automaticamente.
- Não há suporte implementado/validado a USB OTG ou instalação NSP/NSZ/XCI.
- Não apagar arquivos importantes antes de testar cópia e cancelamento com arquivos descartáveis.
- Nenhuma credencial Google deve ser enviada ao repositório.
