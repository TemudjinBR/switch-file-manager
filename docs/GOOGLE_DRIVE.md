# Google Drive — contas e autorização experimental

**Estado: experimental na 0.10.11.** Navegação, transferências e gerenciamento de contas estão implementados e cobertos por API fictícia. O proprietario confirmou acesso Drive, Manter conectado e desconectar/reconectar no Lite com 0.10.10. A nova implementacao de downloads ainda exige validacao fisica; nao se extrapola essa aprovacao para todo o Drive. Não se solicita senha Google no aplicativo, em logs ou ao proprietário.

## Falha de persistência relatada no Switch Lite

Na v0.10.8 foi relatada **“Falha ao salvar login no cartão SD.”** depois da autorização pelo Android. O prefixo antigo “O Google recusou” era incorreto para falha local. A Missão 3 reproduziu a incompatibilidade de rename sobre destino existente e adicionou gravação verificada/recuperação. A chamada/errno exata que falhou no aparelho continua desconhecida. A Missão 3.1 preserva essa correção e acrescenta gerenciamento de contas. Ver [persistência](GOOGLE_DRIVE_PERSISTENCIA.md) e [validação](RELATORIO_VALIDACAO.md).

## Menu Google Drive e conta

Acesse **X → Ações → Google Drive e conta**. O modal existente aceita ↑/↓, ZL/ZR, A/B, toque nas opções e arrasto para rolar.

| Opção | Comportamento |
| --- | --- |
| Entrar com Google | Autoriza pelo navegador Android. Só confirma a conta após gravar e reler o estado completo. |
| Reconectar conta salva | Renova o token local sem exigir nova autorização, quando o Google ainda a permite. Funciona após desconectar ou desativar Manter conectado. |
| Manter conectado | Preferência persistente. Ativada: tenta restaurar sessão na abertura do aplicativo. Desativada: conserva autorização local, mas exige ação explícita para entrar. |
| Abrir o Google Drive neste painel / Voltar este painel ao cartão SD | Altera o painel; voltar ao SD não desconecta. |
| Desconectar conta | Confirmação; encerra sessão e impede restauração automática, sem excluir autorização ou arquivos. |
| Trocar de conta | Confirmação e seletor Google; pausa operações remotas. Cancela sem trocar tokens ou listagens; sucesso limpa os painéis remotos anteriores e abre a raiz nova. |
| Esquecer esta conta | Confirmação distinta; bloqueia restauração e remove dados locais atribuídos à conta, incluindo cópias. Preserva outras contas identificadas, arquivos pessoais e a credencial OAuth compartilhada do aplicativo. |

Estados exibidos: **Conectado**, **Desconectado**, **Autorização necessária**, **Autorização expirada ou inválida**, **Erro de rede** e **Erro de armazenamento no SD**. O nome da conta é consultado em `about.user.displayName`, quando disponível; não se grava identificação nos logs. A consulta opcional não invalida o login se estiver indisponível.

Desconectar/trocar só são acionáveis entre operações: as transferências Drive atuais são síncronas e a fila é somente local. Não existe uma fila remota em segundo plano para cancelar. Durante a autorização de troca, chamadas remotas são recusadas; B restaura a sessão anterior em memória quando ela continuava válida. Se o Google já a invalidou, os arquivos anteriores são preservados, mas não se promete reconexão.

**Esquecer é local.** Não apaga arquivos do Drive e não afirma revogação no Google. Falhas de exclusão ou arquivos sem atribuição segura são informados; o aplicativo mantém o bloqueio de restauração e permite tentar Esquecer novamente. A credencial `google.json` identifica o aplicativo e pode servir a várias contas, portanto é preservada. Não há limpeza automática do diretório inteiro.

## Entrar usando Android

1. Use console e smartphone na mesma rede Wi-Fi confiável; habilite Google Drive API no projeto OAuth individual, com cliente **Aplicativo para computador**. O aplicativo não fornece um client secret público ou serviço pago intermediário.
2. Abra **Entrar com Google**. No Android, acesse a URL local exibida pelo Switch, porta 8080.
3. Se solicitado, selecione o JSON de credencial individual. Nenhuma senha Google deve ser digitada nessa página local.
4. Toque em **Entrar com o Google**, confira o aplicativo/projeto e autorize no site HTTPS do Google. A senha, se exigida, é fornecida somente ao Google.
5. Preserve o fluxo manual existente: quando o navegador tentar abrir localhost, copie a URL completa de retorno, incluindo **code e state**, e cole no formulário local. O código isolado não é aceito nessa revisão.
6. Aguarde a confirmação. Em erro **SD**, use **Tentar salvar login novamente** na mesma sessão; isso repete a gravação local, sem consumir outro código OAuth. Fechar o aplicativo perde o candidato ainda não salvo, preservando os arquivos anteriores.

A preferência Manter conectado é ajustada no menu do Switch. Desconectar não a apaga, mas desativa a restauração da conta até reconexão explícita. Uma autorização nova precisa do próprio refresh token; nunca herda o token de outra conta.

## Compatibilidade com o Google

A revisão consultou a documentação oficial atual: [Desktop Apps](https://developers.google.com/identity/protocols/oauth2/native-app), [migração de loopback](https://developers.google.com/identity/protocols/oauth2/resources/loopback-migration) e [dispositivos com entrada limitada](https://developers.google.com/identity/protocols/oauth2/limited-input-device). PKCE S256 e state foram implementados. Loopback permanece documentado para clientes Desktop; o fluxo de dispositivos limita escopos Drive a appdata/file, que não substituem o acesso existente ao Drive completo.

A transferência manual do retorno entre Android e Switch continua experimental: essas páginas não certificam essa combinação. Não foi alegada conformidade universal nem aceitação pelo Google real; tipo de cliente, escopo, consentimento e políticas da conta ainda podem bloquear autorização. Não houve troca para Device Flow que removesse funcionalidades.

## Dados e compatibilidade

Todos ficam em `sdmc:/switch/switch-manager/`:

- `google.json`: credencial compartilhada do aplicativo; preservada.
- `token.txt` e `login.disabled`: legados, lidos se ainda não existe estado novo; não são alterados implicitamente.
- `drive-account.json`: autorização atual, vínculo com client ID, identificação opcional, preferência e estado de conexão. Tem precedência sobre o legado.
- `.sfm-old-NNN`, `.sfm-new-NNN` e `.tmp.*`: backups/candidatos; também podem conter segredos. Não compartilhar ou incluir em artefatos.

O estado novo é gravado transacionalmente em um único arquivo para evitar mistura entre preferência, conta e token. Esquecimento mantém marcador sem token para impedir retorno ao legado. Detalhes de recuperação, limpeza seletiva e limites em [GOOGLE_DRIVE_PERSISTENCIA.md](GOOGLE_DRIVE_PERSISTENCIA.md).

## Recursos e restrições preservados

Listagem/paginação, download/upload de arquivos e árvores SD↔Drive, criação de pastas, renomeação e lixeira permanecem implementados. Mover SD↔Drive, copiar Drive→Drive, exportar Docs/Planilhas e ZIP remoto continuam indisponíveis. Upload resumível e arquivos muito grandes exigem validação adicional.

## Diagnóstico

- **SD: etapa/errno:** persistência local, sem atribuir rejeição ao Google. Use retry quando oferecido.
- **Google: HTTP/código OAuth:** resposta do endpoint; inclusive invalid_grant preserva dados e informa autorização inválida.
- **Rede:** DNS, transporte ou SSL. A inicialização registra Result em categoria REDE; Result e errno são códigos distintos.
- **Retorno não corresponde:** a URL deve conter o state da autorização atual. Reabra a página e autorize novamente se iniciar uma nova sessão.
- **Versão inesperada:** consulte [NACP e instalação](VERSAO_E_INSTALACAO.md).

Logs em `sdmc:/switch/Switch File Manager/logs/sfm.log`. Nunca enviar tokens, JSON de credencial, códigos ou backups como diagnóstico.

## Downloads na Missao 3.2

A interrupcao por perda de internet falhou no teste fisico da 0.10.10. O codigo anterior removia o parcial ao esgotar tres tentativas imediatas. Na 0.10.11, o destino final e separado dos parciais: checkpoints locais persistem por tarefa/conta e sao conferidos antes de retomar. A versao remota e consultada antes de acrescentar dados ou publicar; mudanca de versao reinicia em outro parcial, sem juntar conteudos.

A UI oferece X/Pausar, X/Continuar e B/Cancelar, tambem por toque. Estados, bytes, velocidade, espera e tentativas aparecem na sobreposicao. Acoes → Downloads do Drive lista tarefas desta conta para recuperacao apos reinicio. Cancelar mantem o parcial; nao ha retomada automatica de tarefa cancelada. Ver [contrato completo](DOWNLOADS_DRIVE.md).

Essas mudancas nao implementam retomada de upload nem alteram a autorizacao aprovada. Tokens/credenciais sao preservados; renovacao usa o fluxo existente.
