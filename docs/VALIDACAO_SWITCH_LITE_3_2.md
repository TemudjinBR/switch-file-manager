# Checklist físico — Missão 3.2, candidata 0.10.12

## Matriz recebida e retificada

O **novo anexo** do proprietário retifica o relato anterior. Não houve acesso da equipe ao console ou SD pessoal.

| Caso na 0.10.10 | Resultado informado |
| --- | --- |
| FTP sozinho | Funcionou |
| FTP antes do Drive | Funcionou |
| FTP após Drive | Funcionou |
| Acesso Drive | Funcionou |
| Manter conectado | Funcionou |
| Desconectar/reconectar | Funcionou |
| FTP + Drive simultâneos | Não funcionou |
| Download retomar após perda de internet | Não funcionou |
| Versão atualizar no Homebrew Menu | Não funcionou |

A Missão 3.1 não está aprovada para produção. O resultado recebido não homologa 0.10.11/0.10.12 nem toda a interface. Preservar as funções aprovadas. Causas físicas da simultaneidade e versão ainda não comprovadas.

## Preparação e registro

Instalar pelo [guia Android](VERSAO_E_INSTALACAO.md#instalação-pelo-android), usando o artefato verde do PR #1. Registrar versão **0.10.12**, build do manifesto, SHA-256 do NRO, caminho em execução quando fornecido, firmware/Atmosphère, modo applet ou memória completa e formato FAT32/exFAT quando conhecidos. Não enviar OAuth, diretórios de configuração ou logs sem revisar dados pessoais.

Usar arquivos/pastas/conta de teste. Não cortar energia, corromper/encher o SD, formatar ou excluir dados pessoais. Testes de falha de armazenamento e encerramento abrupto ficam nas fixtures. Se um item não foi executado, deixar ambas as opções vazias e registrar o motivo. Repetir os cenários de rede em applet/memória completa se ambos disponíveis; resultado num modo não aprova o outro.

## Casos da nova candidata

1. Instalação e identidade: conferir versão Homebrew Menu, cabeçalho/Ações e build do manifesto; abrir caminho e busca de cópias somente leitura. Registrar qualquer divergência sem apagar cópias. **[ ] Funcionou / [ ] Não funcionou**
2. FTP isolado: porta 2121, upload e download de arquivo de teste com comparação do conteúdo; parar/reiniciar. **[ ] Funcionou / [ ] Não funcionou**
3. FTP → Drive: manter FTP ativo, abrir/navegar Drive e continuar acessando/listando via FTP. **[ ] Funcionou / [ ] Não funcionou**
4. Drive → FTP: com Drive conectado, iniciar FTP e verificar acesso aos dois. **[ ] Funcionou / [ ] Não funcionou**
5. Navegação Drive simultânea: navegar pastas enquanto FTP transfere; confirmar bytes dos arquivos, sem travar UI. **[ ] Funcionou / [ ] Não funcionou**
6. Download simultâneo: baixar arquivo Drive enquanto FTP envia/baixa outro arquivo, em caminhos diferentes. Comparar tamanho e hash/conteúdo de ambos. **[ ] Funcionou / [ ] Não funcionou**
7. Upload simultâneo: enviar arquivo Drive enquanto FTP transfere outro; conferir ambos. Não tratar esse caso como retomada de upload. **[ ] Funcionou / [ ] Não funcionou**
8. Reinício independente FTP: durante download Drive, Y para FTP, Y inicia novamente, reconectar cliente; Drive deve continuar. Registrar etapas/códigos se falhar. **[ ] Funcionou / [ ] Não funcionou**
9. Cancelar/retomar Drive com FTP ativo: B cancela download; FTP deve continuar transferindo; abrir Downloads do Drive e continuar explicitamente. **[ ] Funcionou / [ ] Não funcionou**
10. Sair do painel Drive: voltar ao SD sem desconectar conta nem FTP; reabrir Drive e listar. **[ ] Funcionou / [ ] Não funcionou**
11. Perda curta de internet: download em curso mostra interrupção, tentativa/espera e retoma. Conferir arquivo final. Se Wi-Fi inteiro cair, reconectar o cliente FTP, sem esperar sobrevivência da sessão TCP antiga. **[ ] Funcionou / [ ] Não funcionou**
12. Perda longa: esgotar tentativas, verificar parcial pendente e continuar pelo menu após retorno da rede; destino incompleto não deve aparecer como final. **[ ] Funcionou / [ ] Não funcionou**
13. Arquivo grande: se houver espaço, arquivo de teste de cerca de 2 GB interrompido perto de 1,2 GB tenta continuar do prefixo validado; comparar hash final e progresso. **[ ] Funcionou / [ ] Não funcionou**
14. Pausar/continuar: X e toque param os bytes Drive; FTP continua acessível; continuar valida revisão e termina com conteúdo correto. **[ ] Funcionou / [ ] Não funcionou**
15. Cancelar: B/toque preserva arquivos pessoais, outros downloads e destino completo preexistente; tarefa só continua mediante escolha. **[ ] Funcionou / [ ] Não funcionou**
16. Reiniciar aplicativo: pausar/interromper tarefa, fechar normalmente, reabrir e continuar via Downloads do Drive; conferir conteúdo. Não provocar corte elétrico. **[ ] Funcionou / [ ] Não funcionou**
17. Revisão remota: alterar somente arquivo de teste no Drive entre interrupção e retomada; escolher Preservar e voltar, depois reiniciar com segurança; não misturar revisões. **[ ] Funcionou / [ ] Não funcionou**
18. Manter conectado: fechar/reabrir, restaurar conta e listar sem novo consentimento indevido. **[ ] Funcionou / [ ] Não funcionou**
19. Desconectar/reconectar: revalidar fluxo aprovado, sem apagar tokens/configurações e sem derrubar FTP. **[ ] Funcionou / [ ] Não funcionou**
20. Preservação: conferir dois painéis, logo, controles/toque, favoritos/histórico, ZIP e arquivos pessoais intactos. **[ ] Funcionou / [ ] Não funcionou**

Para falha de rede, anotar estágio, Result, errno, pthread quando exibidos, sessões BSD, donos, modo/memória e operação que estava em curso. Identificar claramente o que era simultâneo. Não atribuir falta de memória, bug SSL ou erro de instalação sem evidência. O [relatório](RELATORIO_VALIDACAO.md) registra testes automatizados separadamente.
