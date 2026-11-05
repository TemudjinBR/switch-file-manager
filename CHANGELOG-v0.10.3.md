# Switch File Manager — v0.10.3

## Correções solicitadas (FTP e interface)

- Janela de inicialização do FTP com linhas curtas, legíveis na tela 1280×720 do Switch Lite.
- Janela de status separa endereço, raiz, usuários, número de sessões, listagens, arquivos transferidos e última atividade em linhas próprias.
- Indicador no topo da tela: **FTP ATIVO | clientes | listagens | arquivos**.
- Até quatro sessões FTP simultâneas; um gerenciador Android pode abrir sessão adicional para listar ou enviar arquivos sem bloquear a primeira.
- Compatibilidade com modo **passivo** (PASV/EPSV) e **ativo** (PORT/EPRT); em modo ativo, o endereço de destino deve corresponder ao IP do cliente conectado e não pode usar portas privilegiadas.
- Comandos de listagem LIST/NLST/MLSD e metadados MLST, além de CWD, SIZE, MDTM etc.
- Mantido FTP sem senha, por ativação manual; raiz `sdmc:/`; envio, recebimento e operações em arquivos e pastas.
- Mantida gravação de upload via arquivo temporário e limpeza em caso de falha.
- Encerramento testado com dois clientes simultâneos.
- Versão visível no Homebrew Menu: **0.10.3** (Makefile APP_VERSION).

## Testes realizados

- Compilação **isolada** do arquivo do servidor FTP em Linux com `g++ -std=c++17 -Wall -Wextra -Werror` e simulador mínimo das funções de rede libnx.
- FTP real via `ftplib`: autenticação anônima, listagem de raiz, MLSD, upload, download, sobrescrita, renomeação, exclusão, criação e remoção de pastas.
- Dois clientes conectados simultaneamente; listagens em modos ativo e passivo; transferência cruzada em ambos os modos.
- Encerramento com dois clientes conectados: cerca de 0,21 segundo em ambiente Linux.
- Script de atualização de interface testado em uma amostra da base v0.10.2, inclusive execução repetida sem duplicação.

## Validações ainda necessárias

- Build **real** na imagem devkitPro/devkitA64 via GitHub Actions após publicação (não disponível nesta sessão).
- Teste físico do NRO no Nintendo Switch Lite e no gerenciador FTP Android do proprietário.
- Confirmar se o gerenciador Android usa o endereço, porta 2121 e FTP comum sem criptografia; modo passivo é recomendado e ativo foi adicionado como alternativa.

## Nota de segurança

FTP sem autenticação e sem criptografia expõe os arquivos do cartão SD a qualquer dispositivo capaz de conectar-se ao serviço enquanto estiver ativo. Inicie somente em rede Wi‑Fi confiável, desligue ao terminar e evite apagar pastas de sistema.
