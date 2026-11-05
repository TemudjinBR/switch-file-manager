# Roadmap

## Etapa atual: base reorganizada e apresentação oficial

A Missão 2 preserva a arquitetura e a marca oficial, amplia o catálogo/README e documenta o relato físico do Drive. Não implementa novas funções do aplicativo ou correção OAuth.

## Base da reorganização e estabilização

- Módulos reais e build sem patches de fontes.
- Versionamento unificado e CI independente da publicação.
- Testes de arquivos, FTP, Drive simulado, ZIP, logs e artefatos.
- Documentação de continuidade e preservação dos caminhos legados.

A entrega desta etapa continua experimental até cumprir a matriz física em `TESTES.md`.

## Próximas mudanças pequenas

1. Investigar e corrigir a persistência local do token Drive no Switch Lite, que relata “Falha ao salvar login no cartão SD.”. Instrumentar chamadas e errno sem expor o token; causa exata ainda não confirmada.
2. Validar o NRO no Switch Lite com Atmosphère e registrar firmware, formato do SD e modo do Homebrew Menu.
3. Exercitar transferências prolongadas, clientes simultâneos, cancelamento, SD cheio, retomada FTP e suspensão da aplicação.
4. Modernizar OAuth para um fluxo suportado de aplicativo instalado, avaliando PKCE e retorno sem copiar código manualmente. Preservar compatibilidade dos dados existentes.
5. Reforçar upload resumível Drive, confirmando offsets aceitos pela API e recuperação após desconexão. Testar TLS e contas reais de teste.
6. Extrair modelo de painéis e controladores de ações com testes próprios, mantendo a UI atual.
7. Evoluir logs para diagnóstico consultável na interface e exportação com revisão dos dados pessoais.
8. Planejar migração explícita e reversível de configurações, se necessária, sempre preservando os originais.

## Funcionalidades futuras, fora desta missão

- USB OTG/armazenamento externo conforme suporte real de libnx e Atmosphère.
- Atualização automática com verificação, integridade e recuperação.
- Instalação de pacotes de jogos (NSP/XCI/NSZ), ainda sem implementação; eventual escopo/viabilidade depende de tarefa específica.
- Novos provedores de nuvem e exportação de documentos Google.
- Melhorias visuais depois de regressão documentada da interface atual.

Não há prazo ou funcionalidade prometida sem implementação e validação. Soluções gratuitas e compatibilidade com Switch Lite continuam critérios de escolha.
