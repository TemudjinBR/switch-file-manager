# Versão, NACP e instalação — 0.10.14 experimental

## Identificar o executável real

`VERSION` é a fonte única para versão do cabeçalho, Sobre, NACP principal/auxiliar e pacote. `prepare.py` incorpora build determinístico dos fontes, scripts, headers, Makefile e arte oficial; documentação não altera o identificador. O validador extrai NACP do NRO real, compara versão/build e verifica estrutura. O nome do ZIP sozinho não comprova identidade.

O proprietário observou cópias antigas/ocultas no microSD e versões anteriores exibidas. Arquivos ocultos **não forçam execução por si mesmos**: o Homebrew Menu executa o caminho escolhido. Não atribuir automaticamente a divergência ao usuário, à extensão ou ao cache. Confirmar versão, build, caminho quando o launcher o fornece, NACP e hash da cópia concreta.

**+ Menu → Sistema → Versão e diagnósticos:** Caminho em execução e Conferir cópias no SD. A busca inclui nomes ocultos, usa limites de profundidade/quantidade/bytes e informa cancelamento/busca incompleta. Ao escolher uma cópia, mostra NACP, build e SHA-256 calculado somente por leitura. Falha de hash é informada. Se argv[0] estiver ausente, a UI indica indisponibilidade. Nenhum arquivo é removido.

Também disponível para cópia de teste em nuvem:

```bash
python3 scripts/inspect_installation.py caminho/da/copia-do-SD
```

A ferramenta percorre nomes `.nro`/`.NRO`, inclusive diretórios ocultos, sem seguir links simbólicos de arquivos; confere metadados e hashes. Não acessa o cartão pessoal nem escolhe qual NRO o launcher abriu. Se uma cópia antiga for comprovada, fazer backup e só então decidir manualmente renomear/remover **aquele executável**; não se recomenda limpeza geral.

## Instalação pelo Android

1. Baixar **SwitchFileManager-validado** em Artifacts da execução verde do **novo PR da Missão 3.6**, conferindo SHA da execução. Os artefatos antigos dos PRs #1/#2 não contêm esta correção. GitHub pode exigir login normal para baixar; não criar/fornecer token pessoal.
2. Extrair o ZIP externo de Artifacts e depois **SwitchFileManager-v0.10.14.zip**. Ler LEIA-ME, manifest.json e SHA256SUMS. O NRO avulso é idêntico ao incorporado no ZIP.
3. Fechar o aplicativo. Fazer backup somente dos executáveis anteriores identificados. Copiar a pasta `switch` do pacote para a **raiz do microSD** usando o gerenciador Android e leitor SD. Não copiar a pasta externa do ZIP nem criar um nível extra de diretório.
4. Conferir os dois destinos exatos:

   ```text
   switch/Switch File Manager/SwitchFileManager.nro
   switch/Switch File Manager/SfmUpdateHelper.nro
   ```

5. **Não apagar** `switch/switch-manager/`, google.json, token.txt, estado OAuth, favoritos, histórico, arquivos pessoais ou ocultos. Não é instalação limpa. Não substituir o NRO por download ainda pendente no próprio aplicativo em teste.
6. Reabrir a listagem do Homebrew Menu e escolher o caminho oficial do principal. O auxiliar correspondente só deve ser iniciado pelo fluxo verificado do atualizador, nunca manualmente para teste de menu.
7. **Primeiro teste:** ver **0.10.14**, build igual ao manifesto, rodapé **+ Menu**, **X Ações**, **− Sair**; + deve mostrar cinco categorias, terminando em Sobre. Conferir versão/build em Sobre e caminho em Sistema → Versão e diagnósticos.
8. Se qualquer sinal divergir, interromper a homologação e inspecionar cópias/caminho/hash em modo somente leitura. **Não excluir arquivos automaticamente nem apagar dados para tentar corrigir.**

## Limites

A candidata é experimental, sem homologação física. Atualização automática continua exigindo Release oficial mais nova com update.json e gates existentes; Actions não vira servidor de atualização com token pessoal. O canal de atualizações padrão ESTÁVEL representa a preferência de busca, não uma homologação desta candidata experimental.

A instalação automática segue bloqueada quando argv[0] não corresponde ao destino oficial ou o NRO nesse caminho difere do build em execução. Sobre mostra explicitamente **Canal de atualizações**, separado da versão experimental.

[Checklist físico 3.6](MENU_CONTEXTUAL.md#checklist-no-switch-lite) · [Atualizador](ATUALIZACOES.md) · [Histórico de validação](RELATORIO_VALIDACAO.md).
