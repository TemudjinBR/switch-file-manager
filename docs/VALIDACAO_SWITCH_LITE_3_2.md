# Checklist fisico — Missao 3.2

## Resultado recebido da 0.10.10

Relato do proprietario, sem inferir firmware, formato SD, modo ou hash nao informados:

| Caso | Resultado informado |
| --- | --- |
| Numero de versao exibido mudou para 0.10.10 | Falhou; valor/caminho exatos nao informados nesta rodada |
| FTP isolado | Falhou |
| FTP apos Google Drive | Falhou |
| Acesso Google Drive | Funcionou |
| Manter conectado | Funcionou |
| Desconectar/reconectar | Funcionou |
| Download interrompido por perda de internet retomou | Falhou |

**A Missao 3.1 nao esta aprovada para producao.** Os casos que funcionaram devem ser preservados. Isso nao e um teste fisico executado neste ambiente.

## Nova execucao da candidata 0.10.11

Registrar versao/build do manifesto, hash do NRO, caminho em execucao, firmware/Atmosphere, modo applet/memoria completa e formato SD quando conhecidos. Usar dados/contas de teste e nao provocar corte de energia, corrupcao, formatacao ou exclusao em dados pessoais.

1. Extrair o ZIP experimental e abrir exatamente switch/Switch File Manager/SwitchFileManager.nro. Conferir **0.10.11 e build do manifesto** no cabecalho e no menu Versao e diagnostico do NRO. Abrir Caminho em execucao e Conferir copias no SD. Registrar divergencias; nao apagar nada automaticamente.
2. Iniciar FTP na porta 2121 antes do Drive. Se falhar, registrar texto da propria tela: etapa, Result/errno/pthread e diagnostico detalhado, sem enviar tokens ou configuracoes. Se iniciar, conectar e transferir arquivo pequeno de teste nos dois sentidos, comparar e parar/reiniciar.
3. Repetir FTP depois de acessar Drive, e Drive depois de FTP. Testar Wi-Fi LAN sem internet quando possivel. Apos perda/retorno de Wi-Fi, parar/iniciar FTP e reconectar cliente; nao pressupor que uma sessao TCP anterior sobreviva. Repetir em applet/memoria completa quando disponiveis.
4. Revalidar os casos aprovados: acesso Drive, Manter conectado apos fechar/reabrir, desconectar/reconectar sem novo consentimento desnecessario. Conferir dados pessoais, favoritos/historico e configuracao legada preservados.
5. Baixar arquivo binario de teste maior que alguns MiB. Perder/retomar internet durante download: observar estado, tentativa, espera e continuacao. Comparar tamanho/conteudo/hash final com origem. Repetir perda longa ate esgotar tentativas e usar Downloads do Drive → Continuar.
6. Pausar por X e por toque, conferir que bytes deixam de avancar, continuar e cancelar. Confirmar que o destino incompleto nao aparece como final e que outros arquivos/downloads permanecem intactos.
7. Interromper somente o aplicativo de teste por encerramento normal durante tarefa pendente/pausada, reabrir e continuar pelo menu. Nao usar corte eletrico como teste rotineiro. Conferir que a tarefa pertence a conta correta e que nao mistura arquivos apos trocar de conta.
8. Em arquivo remoto de teste, alterar o conteudo entre interrupcao/continuacao. Conferir aviso de reinicio seguro e conteudo final da nova revisao, sem concatenar partes. Nao modificar arquivos pessoais para simular esse caso.

Marcar cada caso como passou/falhou/nao executado. Uma falha FTP sem etapa/codigo ainda nao comprova sua causa. O build/CI nao substitui esta matriz, e nao se publica Release nesta missao.
