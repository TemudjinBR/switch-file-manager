# Versão, NACP e executável instalado

## Relato e evidência

O proprietário relatou Homebrew Menu exibindo **0.10.8** quando esperava **0.10.9**. O NRO efetivamente produzido/validado na Missão 3 e no Actions 42122757086 contém **0.10.9** no campo DisplayVersion do NACP incorporado. O valor foi extraído do asset, não inferido do nome do ZIP. Portanto não foi comprovado que aquele build gerava 0.10.8; sem o hash/caminho do executável aberto no aparelho, cópia antiga/duplicada e instalação em outro diretório continuam hipóteses.

## Reforços da 0.10.10

- VERSION continua a única fonte. Makefile usa override APP_VERSION para recusar um valor antigo passado ao make, e aborta se a leitura/validação falhar.
- O NACP é gerado para temporário em cada build e comparado por conteúdo. Isso cobre arquivos antigos com timestamps preservados/mais novos; se idêntico, seu timestamp é preservado.
- O NRO depende do NACP, e validate_nro extrai DisplayVersion em 0x3060 a partir do asset ASET e compara com VERSION.
- A suíte extrai diretamente o campo do binário fornecido com --nro e testa recusa de versão divergente. Os testes de Makefile isolam apenas nacptool; a compilação final usa a ferramenta real.
- Menu Ações exibe SFM_VERSION; log APP registra versão e argv[0], quando disponível, para identificar o executável aberto.
- Manifesto e LEIA-ME incluem versão, SHA-256 e caminho SD. NRO, ZIP e sidecars são artefatos experimentais; não se publica Release.

## Instalação identificável

O destino do ZIP, relativo à raiz do SD, é:

```text
switch/Switch File Manager/SwitchFileManager.nro
```

O NRO avulso do artefato é o mesmo incorporado ao ZIP. Extrair a pasta switch do pacote preserva os dados legados em `switch/switch-manager/`. O aplicativo não apaga executáveis em outros caminhos.

Se o Homebrew Menu mostrar versão diferente, conferir o caminho selecionado e a versão do menu Ações. Uma cópia em `switch/SwitchFileManager.nro` ou outra pasta pode continuar disponível. Não é necessário apagar configurações, favoritos, histórico ou tokens para atualizar o executável.

A ferramenta opcional de manutenção é somente leitura:

```bash
python3 scripts/inspect_installation.py caminho/para/copia-do-SD
```

Ela identifica NROs com título Switch File Manager, extrai versões/hashes, sinaliza duplicados e divergência de VERSION; não escolhe qual o Homebrew Menu abriu e não remove arquivos. Pode rodar no ambiente em nuvem sobre uma cópia de teste, sem exigir computador local do proprietário. A missão exercita a ferramenta com fixtures, sem acesso ao cartão pessoal.

## Validação física mínima

Na candidata 0.10.10, conferir versão no Homebrew Menu e no menu Ações para o mesmo NRO. Se divergirem, registrar somente versão/caminho/hash e diagnóstico não secreto, sem compartilhar o diretório OAuth. O relatório físico não foi fabricado nesta missão.

## Identidade independente na 0.10.11

O novo relato fisico informa que a versao da 0.10.10 nao mudou no aparelho; nao trouxe valor NACP/caminho/hash para comprovar a origem. A candidata mostra **SFM_VERSION** e **SFM_BUILD_ID** no cabecalho, alem de Acoes → Versao e diagnostico do NRO. O caminho argv[0] aparece quando fornecido pelo launcher.

prepare calcula uma identidade deterministica vVERSAO-hash a partir dos fontes, headers, Makefile, scripts e icone; documentacao e diretorio de build nao alteram esse identificador. O marcador SFM_BUILD fica incorporado ao executavel. validate_nro e package exigem correspondencia com os fontes atuais; o manifesto/instrucoes do pacote incluem o mesmo build.

A busca no proprio Switch e somente leitura, restrita a sdmc:/switch, profundidade/entradas/copias/bytes limitados. Confere NRO/ASET/NACP e procura o marcador de build com memoria limitada. NROs antigos podem mostrar build indisponivel. Uma busca limitada/cancelada e identificada, sem alegar inventario integral. Nenhuma copia e removida. A versao do NACP de cada copia e mostrada separadamente da identidade em execucao.

Instalacao da candidata anterior (0.10.11): extraia switch/Switch File Manager/SwitchFileManager.nro do ZIP experimental na raiz do SD, abra exatamente essa copia no Homebrew Menu e confira **0.10.11 mais o build do manifesto** no aplicativo. Se divergir, a identidade apresentada nao corresponde ao pacote; consulte caminho/cópias na UI. Nao apague google.json, token.txt ou o diretorio legado para atualizar.

## Instalação pelo Android

A candidata anterior desta seção era **0.10.12 experimental**; o pacote contém o build e o SHA-256 específicos da execução Actions. O NACP da 0.10.11 anterior também era validado: não foi comprovado por que o Homebrew Menu abriu/exibiu outra versão. Não presumir erro de instalação do proprietário.

1. No navegador Android autenticado no GitHub, abrir a execução verde vinculada ao PR #1 e baixar **SwitchFileManager-validado** em Artifacts. É necessário login GitHub para baixar artefatos, sem criar/fornecer token pessoal.
2. Extrair o ZIP de artefato. Dentro dele, extrair **SwitchFileManager-v0.10.12.zip** e ler LEIA-ME/manifesto. Evitar copiar o ZIP externo como se fosse aplicativo.
3. Fechar o Switch File Manager antes de substituir seu executável. Pelo gerenciador Android com leitor microSD, copiar a pasta `switch` do pacote para a raiz do SD. Como alternativa, usar um cliente FTP Android e outro servidor FTP já disponível no console; transferir o NRO completo para o destino oficial. Não substituir o NRO por meio de uma transferência ainda pendente do próprio aplicativo em teste.
4. Destino exato: `sdmc:/switch/Switch File Manager/SwitchFileManager.nro`. Não criar uma pasta extra com o nome do ZIP; não mover/apagar `switch/switch-manager/`, tokens, configurações, favoritos ou histórico.
5. Reabrir/atualizar a listagem do Homebrew Menu e executar essa cópia. Conferir **0.10.12**, build igual ao manifesto em Ações → Versão e diagnóstico do NRO, e Caminho em execução quando disponível. O caminho pode não ser fornecido pelo launcher; a UI informa essa ausência.
6. Se divergir, usar Conferir cópias no SD (somente leitura) e registrar versão/build/caminho mostrados. O diagnóstico não exclui cópias automaticamente. Conferir SHA-256 com ferramenta Android que suporte hash, se disponível; nome do arquivo sozinho não comprova identidade.

A instalação não requer computador local nem limpeza de dados. A versão incorporada/NACP/ícone e o empacotamento são conferidos automaticamente; o valor exibido pelo launcher e a cópia efetivamente aberta precisam do teste físico.

## Candidata 0.10.13 e auxiliar

Baixar o artefato verde do PR #1 e extrair o ZIP externo, depois **SwitchFileManager-v0.10.13.zip**. Fechar o aplicativo e copiar a pasta switch para a raiz do SD pelo Android, preservando configurações. O pacote agora inclui **SwitchFileManager.nro** e **SfmUpdateHelper.nro** no diretório oficial. O auxiliar empacotado não deve ser aberto isoladamente como se fosse o aplicativo: a atualização usa um auxiliar verificado dentro da transação específica.

Conferir 0.10.13/build do manifesto e caminho em execução. Atualização pelo app precisa de Release oficial mais nova com update.json; não usar tokens pessoais para baixar artefatos Actions no Switch. A instalação automática fica bloqueada se argv[0] não for exatamente sdmc:/switch/Switch File Manager/SwitchFileManager.nro ou se o NRO nesse caminho não corresponder ao build em execução. Duplicados continuam intactos.
