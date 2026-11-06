# Roadmap

## Etapa atual: atualizador experimental 0.10.13 e gates de distribuição

As Missões 1/2/3 continuam preservadas. A Missão 3.1 acrescenta ciclo compartilhado de sockets, versão identificável e gerenciamento de contas com PKCE/state e persistência verificada. O relato “Falha ao salvar login no cartão SD.” é histórico: o anexo posterior aprova acesso/persistência/reconexão Drive e FTP isolado/sequencial, mas reprova simultaneidade, retomada e versão exibida.

## Base da reorganização e estabilização

- Módulos reais e build sem patches de fontes.
- Versionamento unificado e CI independente da publicação.
- Testes de arquivos, FTP, Drive simulado, ZIP, logs e artefatos.
- Documentação de continuidade e preservação dos caminhos legados.

A entrega desta etapa continua experimental até cumprir a matriz física em `TESTES.md`.

## Próximas mudanças pequenas

1. Validar fisicamente a candidata 0.10.12: sobreposição real FTP + Drive, recuperação de downloads e identidade. Preservar/revalidar o que já funcionou; registrar etapas/códigos sem presumir causa única.
2. Validar o NRO no Switch Lite com Atmosphère e registrar firmware, formato do SD e modo do Homebrew Menu.
3. Exercitar transferências prolongadas, clientes simultâneos, cancelamento, SD cheio, retomada FTP e suspensão da aplicação.
4. Validar OAuth com o Google real e avaliar retorno sem cópia manual compatível com os escopos do gerenciador. PKCE/state já foram implementados; não confundir isso com homologação do fluxo Android↔Switch.
5. Reforçar upload resumível Drive, confirmando offsets aceitos pela API e recuperação após desconexão. Testar TLS e contas reais de teste.
6. Extrair modelo de painéis e controladores de ações com testes próprios, mantendo a UI atual.
7. Evoluir logs para diagnóstico consultável na interface e exportação com revisão dos dados pessoais.
8. Planejar migração explícita e reversível de configurações, se necessária, sempre preservando os originais.

## Funcionalidades futuras, fora desta missão

- USB OTG/armazenamento externo conforme suporte real de libnx e Atmosphère.
- Homologar o atualizador implementado nesta missão; assinatura independente de manifesto permanece planejada.
- Instalação de pacotes de jogos (NSP/XCI/NSZ), ainda sem implementação; eventual escopo/viabilidade depende de tarefa específica.
- Novos provedores de nuvem e exportação de documentos Google.
- Melhorias visuais depois de regressão documentada da interface atual.

Não há prazo ou funcionalidade prometida sem implementação e validação. Soluções gratuitas e compatibilidade com Switch Lite continuam critérios de escolha.

## Prioridade apos a Missao 3.2

Executar o checklist fisico da 0.10.12, sem presumir resolucao da simultaneidade/versao. “Falha ao salvar login no cartão SD.” e um relato historico: o acesso/Manter conectado/reconexao funcionaram na 0.10.10 relatada. Preservar esses resultados, validar os novos downloads e registrar etapa/codigo se FTP falhar. Retomada de upload permanece fora do escopo.

## Prioridade da Missão 3.3

Atualizador/auxiliar/canais estão implementados e em teste; assinatura independente continua planejada. A prioridade é homologar o [checklist 3.3](VALIDACAO_SWITCH_LITE_3_3.md), revalidar funções aprovadas e aplicar proteção administrativa real da main antes da primeira promoção. Nenhuma aprovação física é criada pela automação.
