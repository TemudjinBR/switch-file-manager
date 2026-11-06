# Roadmap

## Etapa atual: FTP diagnosticavel, downloads recuperaveis e identidade 0.10.11

As Missões 1/2/3 continuam preservadas. A Missão 3.1 acrescenta ciclo compartilhado de sockets, versão identificável e gerenciamento de contas com PKCE/state e persistência verificada. A confirmação no Switch Lite ainda depende de teste físico para o relato “Falha ao salvar login no cartão SD.”.

## Base da reorganização e estabilização

- Módulos reais e build sem patches de fontes.
- Versionamento unificado e CI independente da publicação.
- Testes de arquivos, FTP, Drive simulado, ZIP, logs e artefatos.
- Documentação de continuidade e preservação dos caminhos legados.

A entrega desta etapa continua experimental até cumprir a matriz física em `TESTES.md`.

## Próximas mudanças pequenas

1. Validar fisicamente a persistência/contas candidatas da v0.10.10 e identificar a chamada/errno caso o erro continue. Não presumir que a reprodução de EEXIST explica todos os cenários do SD.
2. Validar o NRO no Switch Lite com Atmosphère e registrar firmware, formato do SD e modo do Homebrew Menu.
3. Exercitar transferências prolongadas, clientes simultâneos, cancelamento, SD cheio, retomada FTP e suspensão da aplicação.
4. Validar OAuth com o Google real e avaliar retorno sem cópia manual compatível com os escopos do gerenciador. PKCE/state já foram implementados; não confundir isso com homologação do fluxo Android↔Switch.
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

## Prioridade apos a Missao 3.2

Executar o checklist fisico da 0.10.11, sem presumir resolucao de FTP/versao. “Falha ao salvar login no cartão SD.” e um relato historico: o acesso/Manter conectado/reconexao funcionaram na 0.10.10 relatada. Preservar esses resultados, validar os novos downloads e registrar etapa/codigo se FTP falhar. Retomada de upload permanece fora do escopo.
