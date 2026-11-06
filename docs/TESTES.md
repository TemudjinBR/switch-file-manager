# Testes e validação

## Separar execuções e relatos

Os resultados da Missão 1 foram preservados em [histórico](historico/VALIDACAO_MISSAO_1.md). A [Missão 2](historico/VALIDACAO_MISSAO_2.md) preserva sua validação anterior; o [relatório atual](RELATORIO_VALIDACAO.md) identifica a execução da Missão 3.1, sobre FTP, versão e contas OAuth. A [Missão 3](historico/VALIDACAO_MISSAO_3.md) também foi preservada em histórico. O relato do proprietário sobre “Falha ao salvar login no cartão SD.” no Lite é registrado como falha física conhecida, sem supor que a equipe executou ou aprovou a matriz no console.

## Execução reproduzível

A suíte usa Python 3, GCC/G++ C++17 e bibliotecas padrão. Compila os módulos de produção em executáveis temporários no host e usa sockets de loopback. Não acessa conta Google, rede externa ou SD de usuário.

```bash
python3 tests/run.py
bash scripts/build.sh
```

Sem `--nro`, o teste do artefato compilado fica explicitamente ignorado. Depois da compilação real:

```bash
python3 scripts/validate_nro.py SwitchFileManager.nro
python3 tests/run.py --nro SwitchFileManager.nro
python3 scripts/package.py
(cd dist && sha256sum --check SwitchFileManager.nro.sha256)
(cd dist && sha256sum --check SwitchFileManager-v0.10.11.zip.sha256)
```

`bash scripts/build.sh` reúne testes, build, validação e pacote. No ambiente desta auditoria utiliza `/workspace/sfm-environment/build.sh`, que compila uma cópia descartável com a imagem devkitPro configurada. O build nativo também funciona com as variáveis DEVKITPRO/DEVKITA64 do README.

## Cobertura automatizada

| Área | Evidência executada |
| --- | --- |
| Versão e preparação | Versões inválidas recusadas; cabeçalho atualizado quando necessário; repetição sem alterar timestamp; entradas legadas executadas duas vezes sem modificar fontes |
| Estrutura e documentação | Módulos não vazios, arquivos obrigatórios, assets, terceiros, links locais e sintaxe dos scripts |
| Arquivos locais | Cópia de árvore com arquivos reais, comparação de conteúdo, detecção de corrupção, remoção, cancelamento e preservação de destino existente |
| Nomes e caminhos | Raiz SD, caminho legado, nomes FAT/exFAT, UTF-8 completo e rejeição de traversal ZIP |
| Logs | Criação, append, linha única, 400 gravações concorrentes, rotação, diretório inválido e `/dev/full` para erro de flush |
| ZIP | Compactação/extração com miniz real e comparação via `zipfile`, pastas vazias, ZIP inválido, cancelamento, traversal e falha de gravação final |
| FTP | Processo do servidor real com sockets POSIX, modo ativo/passivo, upload/download, REST, nomes, pastas, renomeação/exclusão, destino temporário existente e rejeição de alvo ativo inválido |
| Google Drive | Credenciais fictícias, renovação simulada, paginação, download, upload resumível básico, HTTP 401 com renovação, 403, 416, resposta incompleta e destino existente |
| Persistência OAuth | Substituição sem sobrescrita no rename, escrita parcial, EINTR, zero bytes, ENOSPC, EROFS, EIO, sincronização indisponível, rollback, recuperação após saída abrupta, preservação dos anteriores e limite de slots |
| Confirmação de login | Primeiro login, renovação/rotação, releitura obrigatória, retry local sem nova autorização, logout sem exclusão e erros SD/Google separados, com dados fictícios |
| Servidor OAuth local | Formulário de credenciais/código e erro de armazenamento; somente dados fictícios |
| NRO | Assinatura, segmentos, limites ASET, ícone, NACP, título, versão e rejeição de artefatos truncados/alterados |
| Distribuição | Lista fechada de arquivos, hashes, conteúdo NRO, manifesto, ZIP íntegro e resultado idêntico com o mesmo NRO |

`tests/stubs/switch.h` substitui somente inicialização/finalização de sockets no teste FTP. `HOST_TEST` usa endereço loopback e porta efêmera. Produção mantém libnx, porta 2121 e descoberta de IP reais. Drive usa seu ponto de teste HOST_TEST já existente; TLS do console não é simulado.

A suíte não valida pixels da interface, eventos do controle físico, serviços SSL, desempenho do Switch ou a política OAuth do Google. A matriz abaixo é obrigatória para publicação.

## Regressão estrutural

Depois de mover fontes ou alterar caminhos:

1. Executar toda a suíte existente e corrigir qualquer falha pela causa.
2. Compilar com devkitPro, sem reaproveitar binário de uma etapa anterior.
3. Validar o NRO e rodar `tests/run.py --nro`.
4. Repetir `make` no mesmo diretório e conferir que não reescreve fontes; após `VERSION` mudar, conferir o novo NACP.
5. Conferir `git diff --check`, arquivos da distribuição e checksums.

O build fixa a imagem por digest. Os avisos restantes de funções estáticas não usadas vêm de stb_truetype; a mensagem miniz alerta sobre suporte de grandes arquivos. Não são erros de compilação, nem evidência de suporte a arquivos maiores que 4 GiB.

## Matriz obrigatória no Switch físico

Criar `docs/validacao-console/vX.Y.Z.md` somente após execução real, com dispositivo, firmware, versão do Atmosphère, formato do SD, modo de execução, versão/commit e SHA-256 do NRO. Registrar cada caso como passou, falhou ou não executado, com observações reproduzíveis:

- Inicialização pelo Homebrew Menu em Switch Lite; testar também diferenças de modo applet/título quando disponíveis.
- Dois painéis, fontes, toque, rolagem, seleção, X/Y/A/B/L/R/ZL/ZR/−/+ e saída sem travamento.
- Criar, copiar, mover, renomear e apagar arquivos/pastas; favoritos/histórico existentes continuam legíveis.
- Cancelar cópias e transferências; provocar destino indisponível/SD cheio usando somente dados de teste; conferir preservação da origem.
- FTP ativo/passivo, arquivos pequenos/grandes, REST de download, clientes simultâneos, parada com sessão aberta e perda de Wi-Fi.
- Google Drive com credenciais de teste: login real, renovação, listagem paginada, download/upload, cancelamento, renomeação e lixeira.
- ZIP de pasta vazia, nomes acentuados, compressão, extração, CRC, cancelamento e limites FAT32/exFAT.
- Logs no caminho com espaços, rotação e diagnóstico de falha; revisar se contêm dados pessoais antes de compartilhar.
- Fila de transferências, abertura de outro NRO, Home/suspensão e retorno conforme suporte do ambiente.

Não executar testes destrutivos sobre dados de usuário. Arquivos >4 GiB exigem exFAT e teste específico; não declarar esse cenário aprovado por extrapolação.

## Critérios de Release

- PR revisado, suíte verde e NRO compilado da revisão exata da tag.
- `VERSION` corresponde à tag `vX.Y.Z`; notas específicas existem.
- Matriz física registrada, falhas relevantes resolvidas ou limitações explicitamente aceitas e publicadas.
- SHA-256 conferido e pacote sem configurações, tokens ou dados pessoais.
- Executar manualmente `Publicar versão validada` com a tag existente e confirmação de console. O workflow exige o relatório físico e recompila/testa antes de publicar.
- Recusar Release existente; não sobrescrever artefatos ou tags já distribuídos.

Nesta missão não se criou o relatório físico ficticiamente nem se publicou Release. Os resultados desta execução estão em [RELATORIO_VALIDACAO.md](RELATORIO_VALIDACAO.md).

## Validação da apresentação — Missão 2

```bash
python3 scripts/check_docs.py
python3 tests/run.py
bash scripts/build.sh
```

O verificador de documentação não depende de pacotes Python externos. Ele examina Markdown e HTML dos documentos ativos, caminhos, âncoras locais e imagens. Os testes adicionais conferem sua reação a links/âncoras quebrados, a preservação do ícone oficial, o uso desse ícone no README e no NRO quando fornecido, a versão e a cobertura dos rótulos dos submenus inventariados.

Links externos importantes são conferidos separadamente por requisição de leitura; uma resposta de GitHub pode exigir autenticação em um artefato de Actions, e isso não equivale a link inexistente. Resultados e limites externos devem ser identificados no relatório da missão.

A revisão visual abre a página renderizada em Chromium nos tamanhos de desktop e smartphone, mede overflow e carregamento do logotipo e inspeciona capturas da documentação. Não usar esses arquivos como imagens do aplicativo. Uma prévia local com CSS compatível não é uma captura da página renderizada pelo servidor do GitHub.

Revisão opcional de estilo Markdown, com versão fixada e configuração do repositório:

```bash
npm exec --yes --package markdownlint-cli2@0.18.1 -- markdownlint-cli2 README.md CHANGELOG.md 'docs/*.md' 'docs/screenshots/*.md' 'docs/historico/*.md' 'assets/branding/*.md' 'third_party/*.md'
```

A configuração aceita os elementos HTML do cabeçalho, permite a primeira linha com logotipo antes do H1 e não impõe limite de linha a parágrafos/tabelas longos. Isso não desativa as verificações de links, imagens, âncoras ou hierarquia do restante do Markdown.

## Regressão de persistência — Missão 3

`tests/test_credential_store.py` compila o armazenamento de produção com injeção de falhas somente em `HOST_TEST`. Os hooks mantêm operações reais em diretórios temporários; não acessam o SD. `tests/host/fsdev_rename.cpp` recusa destino existente com EEXIST, reproduzindo a diferença relevante de libnx em relação ao POSIX do host. A saída abrupta entre as duas renomeações é simulada em subprocesso e seguida de nova carga. Isso não simula perda elétrica do controlador do cartão.

`tests/test_network.py` exercita o servidor de login e o endpoint de tokens local, incluindo falhas de armazenamento após HTTP 200. Confere `isLoggedIn()`, arquivos, marcador de desconexão e mensagens; respostas simuladas não provam aprovação pelo Google real. Diagnósticos são examinados para ausência dos valores fictícios usados como segredos.

Na matriz física do Drive, registrar também: primeira autorização, substituição de token já existente, encerramento/reabertura, renovação, nova tentativa local e desconexão persistente. Usar cartão/conta de teste; não provocar falhas destrutivas no SD do proprietário. Conferir `DRIVE_SD` com etapa/errno em `sdmc:/switch/Switch File Manager/logs/`. Detalhes em [persistência](GOOGLE_DRIVE_PERSISTENCIA.md).

## Regressão de rede, versão e contas — Missão 3.1

| Área | Teste e limite |
| --- | --- |
| Ciclo de sockets | test_network_lifecycle compila FTP/Drive/runtime reais; o stub reproduz inicialização duplicada e Result de falha. Transferências reais de loopback antes/depois de parar o outro consumidor; retry, parada/reinício e idempotência. Não simula SSL, applet ou memória do Switch. |
| Contas | test_network usa HTTP fictício e E/S temporária: restauração, keep on/off, reconexão, troca, cancelamento, suspensão de acesso, vínculo client ID, identidade e ausência de mistura de tokens. |
| Exclusão explícita | Marcador antes de apagar, falhas SD/EROFS, retomada após cópias já removidas, temporários desconhecidos preservados, favoritos/histórico e outras contas intactos; nenhuma revogação remota alegada. |
| OAuth | PKCE S256 contra vetor RFC 7636/hashlib, padding SHA-256, nonces, csrf e state incompatível sem consumir código; sem senhas/credenciais reais. |
| Versão | Makefile real com fronteira nacptool fictícia testa override antigo, timestamp enganoso, repetição e versão inválida. Com --nro, extrai DisplayVersion diretamente do asset compilado e compara VERSION. |
| Instalação/artefatos | Detecta cópia antiga em fixture sem modificá-la; verifica manifesto/caminho, checksums, lista permitida e ausência de segredos fictícios no pacote. |

A compilação final usa nacptool real. Os testes de protocolo e armazenamento complementam, sem substituir, o NRO nativo e a matriz física.

### Procedimento mínimo no Lite para esta candidata

1. Conferir 0.10.10 no Homebrew Menu e no menu Ações, para o caminho indicado no pacote. Registrar firmware/Atmosphère, modo applet ou memória completa, formato SD e hash do NRO.
2. Com dados de teste, iniciar FTP, conectar na porta 2121, enviar/baixar e comparar arquivo, parar e reiniciar. Repetir após usar Drive; testar também FTP ativo antes de entrar no Drive. Repetir em applet/memória completa se disponíveis, registrando Result real se falhar.
3. Autorizar conta de teste pelo Android, confirmar persistência e reabrir aplicativo com keep ativado/desativado. Reconectar, renovar, desconectar, trocar para outra conta e cancelar uma troca; conferir ausência de listagens antigas. Testar Esquecer somente em conta de teste, verificando aviso local/ausência de revogação alegada e preservação dos arquivos pessoais.

Não provocar corte de energia, SD cheio/corrupção ou exclusão em dados do proprietário. Os cenários de falha já usam diretórios temporários e hooks no host. FAT32/exFAT real, UI/toque/controles e autorização Google real continuam não executados nesta missão.

## Missao 3.2

- test_download_manager: E/S temporaria real, CRC, interrupcao em checkpoint/publicacao/pausa, queda de conexao, Range recusado, versao alterada, cancelamento, destino concorrente e erros SD.
- test_network: HTTP loopback com corpo cortado, Range real, arquivo alterado, resposta 200 ignorando Range, 401 com renovacao e todas as regressoes de autenticacao.
- test_network_lifecycle: Wi-Fi simulado cai/retorna e falhas por etapa, alem de FTP↔Drive e transferencias reais loopback. A fronteira NIFM/SDK nao representa hardware.
- test_build_info: identidade compilada, copias antigas, NRO invalido, links simbolicos, busca cancelada somente leitura e recusa de build diferente com mesma versao.
- test_io_wait: WouldBlock, timeout, cancelamento e propagacao imediata de erro de certificado, sem simular um certificado real do console.

A matriz fisica corrente esta em [VALIDACAO_SWITCH_LITE_3_2.md](VALIDACAO_SWITCH_LITE_3_2.md). A validacao recebida da 0.10.10 e identificada como relato do proprietario, sem inventar firmware, formato do SD, hash/caminho ou teste executado pela equipe.
