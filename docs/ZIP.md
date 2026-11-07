# Compactação, inspeção e extração ZIP

Estado: **implementado**, com miniz real nos testes de host. Integração no console, arquivos grandes e ZIP64 permanecem sem validação física suficiente. Fontes: [zipx.cpp](../src/archives/zipx.cpp), [políticas de caminhos](../src/security/path_policy.cpp) e `extractSelectedZips`/`doExtractZip`/`doCompressZip` em [main.cpp](../src/app/main.cpp).

## Usar o menu

Com itens locais selecionados, **X → Compactar em ZIP** abre Cancelar, Rápida e Sem compressão (níveis 1 e 0). Reúne arquivos/pastas em um ZIP, preservando o motor existente.

Quando toda a seleção é ZIP, aparece **Extrair ZIP** ou **Extrair ZIPs**. O diálogo oferece Cancelar, Na pasta do painel ativo e Na pasta do outro painel; cada ZIP gera uma pasta exclusiva, e o destino precisa estar no microSD.

**A** em um ZIP local mantém Cancelar, Extrair aqui, Extrair no outro painel e Informações. A inspeção mostra arquivos, pastas, tamanho descompactado e indicação de senha. A API admite outros níveis de compressão, mas a UI só oferece rápido/sem compressão.

## Verificações e cancelamento

A extração rejeita caminhos absolutos e componentes `..`, normaliza separadores e saneia nomes FAT/exFAT. Itens com senha, método não suportado ou caminho inseguro são ignorados e contabilizados. Detectar criptografia não significa conseguir descriptografar: não há solicitação/suporte a senha para extrair esses itens.

O iterador de extração confere CRC; erros de escrita, flush e fechamento são tratados. B solicita cancelamento pelas pontes de progresso, e a aplicação tenta limpar o destino parcial. Confirmação de CRC é diferente de transação ou garantia contra falha elétrica.

## Limites

- Não opera diretamente no Drive. Baixar antes e usar o SD.
- O alvo é SD FAT/exFAT; validação de componentes não constitui sandbox genérico contra symlinks já existentes em qualquer filesystem do host.
- FAT32 limita cada arquivo a menos de 4 GiB. A biblioteca tem lógica de ZIP64, mas isso não comprova compatibilidade de todos os arquivos, cartões e toolchains; manter o cenário como pendente de teste específico.
- Tamanho de progresso é o total descompactado/estimado; itens ignorados podem alterar a leitura prática do percentual.
- Os testes fazem roundtrip com conteúdo real e pastas vazias, comparam bytes, rejeitam traversal, ZIP inválido e cancelamento, e provocam erro de finalização com `/dev/full`. Não são testes de desempenho do Switch.
