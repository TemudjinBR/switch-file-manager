# Compactação, inspeção e extração ZIP

Estado: **implementado**, com miniz real nos testes de host. Integração no console, arquivos grandes e ZIP64 permanecem sem validação física suficiente. Fontes: [zipx.cpp](../src/archives/zipx.cpp), [políticas de caminhos](../src/security/path_policy.cpp) e `zipMenu`/`doExtractZip`/`doCompressZip` em [main.cpp](../src/app/main.cpp).

## Usar o menu

Em **X → Ações → Compactar e extrair (ZIP)**:

- **Compactar a seleção (rápido):** arquivos, pastas e seleções múltiplas locais, nível 1 da biblioteca.
- **Compactar a seleção (sem compressão):** mesmos tipos de entrada, nível 0.
- **Extrair o ZIP selecionado aqui:** cria uma pasta com nome disponível na pasta atual.
- **Extrair o ZIP selecionado no outro painel:** o painel de destino precisa estar no SD.
- **Informações do ZIP:** quantidade de arquivos, pastas, tamanho descompactado e indicação de itens protegidos por senha.

A em um `.zip` local também abre opções de extração/inspeção. A API de compressão admite outros níveis, mas a UI atual oferece somente rápido e sem compressão; não anunciar um terceiro modo no menu.

## Verificações e cancelamento

A extração rejeita caminhos absolutos e componentes `..`, normaliza separadores e saneia nomes FAT/exFAT. Itens com senha, método não suportado ou caminho inseguro são ignorados e contabilizados. Detectar criptografia não significa conseguir descriptografar: não há solicitação/suporte a senha para extrair esses itens.

O iterador de extração confere CRC; erros de escrita, flush e fechamento são tratados. B solicita cancelamento pelas pontes de progresso, e a aplicação tenta limpar o destino parcial. Confirmação de CRC é diferente de transação ou garantia contra falha elétrica.

## Limites

- Não opera diretamente no Drive. Baixar antes e usar o SD.
- O alvo é SD FAT/exFAT; validação de componentes não constitui sandbox genérico contra symlinks já existentes em qualquer filesystem do host.
- FAT32 limita cada arquivo a menos de 4 GiB. A biblioteca tem lógica de ZIP64, mas isso não comprova compatibilidade de todos os arquivos, cartões e toolchains; manter o cenário como pendente de teste específico.
- Tamanho de progresso é o total descompactado/estimado; itens ignorados podem alterar a leitura prática do percentual.
- Os testes fazem roundtrip com conteúdo real e pastas vazias, comparam bytes, rejeitam traversal, ZIP inválido e cancelamento, e provocam erro de finalização com `/dev/full`. Não são testes de desempenho do Switch.
