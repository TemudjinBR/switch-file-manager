# Testes e validação

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
(cd dist && sha256sum --check SwitchFileManager-v0.10.8.zip.sha256)
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
