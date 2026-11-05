# Dependências incorporadas

| Dependência | Uso | Licença e origem da cópia |
| --- | --- | --- |
| `miniz/miniz.c`, `miniz/miniz.h` | ZIP/deflate/inflate | Cabeçalho identifica miniz 3.0.0; declaração de domínio público/Unlicense nos próprios arquivos |
| `stb/stb_truetype.h` | Rasterização da fonte do sistema | stb_truetype 1.26; opções MIT/domínio público no próprio arquivo |

Os bytes dessas dependências foram preservados durante a reorganização. Não foram baixadas versões novas nem aplicadas correções silenciosas nos terceiros. A fonte consumida por stb é a fonte do sistema; não há suporte novo a fontes externas não confiáveis.

libnx e devkitA64 são fornecidos pelo ambiente devkitPro. O workflow fixa a imagem por digest para manter as ferramentas desta auditoria; atualizações desse digest devem passar por nova compilação e testes.
