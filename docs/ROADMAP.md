# Roadmap

## Etapa atual: reorganização e estabilização

- Módulos reais e build sem patches de fontes.
- Versionamento unificado e CI independente da publicação.
- Testes de arquivos, FTP, Drive simulado, ZIP, logs e artefatos.
- Documentação de continuidade e preservação dos caminhos legados.

A entrega desta etapa continua experimental até cumprir a matriz física em `TESTES.md`.

## Próximas mudanças pequenas

1. Validar o NRO no Switch Lite com Atmosphère e registrar firmware, formato do SD e modo do Homebrew Menu.
2. Exercitar transferências prolongadas, clientes simultâneos, cancelamento, SD cheio, retomada FTP e suspensão da aplicação.
3. Modernizar OAuth para um fluxo suportado de aplicativo instalado, avaliando PKCE e retorno sem copiar código manualmente. Preservar compatibilidade dos dados existentes.
4. Reforçar upload resumível Drive, confirmando offsets aceitos pela API e recuperação após desconexão. Testar TLS e contas reais de teste.
5. Extrair modelo de painéis e controladores de ações com testes próprios, mantendo a UI atual.
6. Evoluir logs para diagnóstico consultável na interface e exportação com revisão dos dados pessoais.
7. Planejar migração explícita e reversível de configurações, se necessária, sempre preservando os originais.

## Funcionalidades futuras, fora desta missão

- USB OTG/armazenamento externo conforme suporte real de libnx e Atmosphère.
- Atualização automática com verificação, integridade e recuperação.
- Novos provedores de nuvem e exportação de documentos Google.
- Melhorias visuais depois de regressão documentada da interface atual.

Não há prazo ou funcionalidade prometida sem implementação e validação. Soluções gratuitas e compatibilidade com Switch Lite continuam critérios de escolha.
