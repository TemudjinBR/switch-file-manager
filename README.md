# Switch File Manager v0.10.7

Correção experimental: buffers FTP de 128 KiB movidos da pilha para o heap.
Registro persistente de inicialização, encerramento, ações e eventos FTP.
Local: `sdmc:/switch/SwitchFileManager/logs/sfm.log`.

Os eventos são gravados imediatamente. Uma falha fatal pode impedir o registro do último evento; consulte também relatórios de crash do Atmosphère.
O log pode conter nomes e caminhos de arquivos. Revise antes de compartilhar.

Instalação: envie os quatro arquivos do ZIP à raiz do repositório. Mantenha scripts anteriores.
Validação: sintaxe Python e ZIP verificados; compilação devkitA64 e teste físico pendentes.
