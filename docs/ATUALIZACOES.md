# Atualizações no console — 0.10.13 experimental

## Estado e uso

Implementado e testável no host; **sem homologação física**. A instalação inicial da candidata continua pelo [pacote Android](VERSAO_E_INSTALACAO.md#candidata-01013-e-auxiliar). Nenhuma Release é publicada nesta missão. Enquanto o repositório não tiver Releases compatíveis com `update.json`, a consulta não encontrará uma candidata instalável. A versão pública anterior pode não ter esse manifesto.

Em **Ações → Atualizações**, por controles ou toque:

1. Conferir versão instalada, canal e estado. O diálogo atualiza estado/bytes enquanto o worker trabalha; notas têm páginas próprias.
2. ESTÁVEL é o padrão. Mudar para EXPERIMENTAL exige escolha no diálogo que informa ausência de homologação.
3. Verificar manualmente ou habilitar/desabilitar checagem ao abrir. Rede/HTTPS rodam em thread própria, sem executar APIs Drive nem desenhar fora da thread da UI.
4. Baixar mediante confirmação. O download grava novos arquivos exclusivos em uma área gerenciada; não altera o executável instalado.
5. Instalar e reabrir exige outra confirmação e `envHasNextLoad()`. O aplicativo para worker/FTP/Drive, fecha a UI e pede ao launcher para abrir o auxiliar verificado. Só o auxiliar troca o NRO principal.
6. Recuperar versão anterior é uma ação explícita, com confirmação e verificação do backup. Não é downgrade automático.

O worker permite cancelamento pelo item **Cancelar tarefa**. Sair do diálogo por B não cancela silenciosamente a tarefa em segundo plano. O programa pede encerramento do worker antes de sair; DNS síncrono do SDK ainda pode atrasar esse encerramento. E/S do SD e criação de thread não são operações sem latência.

## Fontes, canais e limites de API

A consulta anônima usa `https://api.github.com/repos/TemudjinBR/switch-file-manager/releases/latest` para estável e as dez Releases mais recentes para experimental. `draft` é recusado; `prerelease` precisa corresponder ao canal. Só versões numéricas X.Y.Z superiores à instalada são oferecidas; mesma versão com outro build não substitui o NRO. Voltar de uma experimental numericamente maior para estável menor exige uma instalação manual consciente ou rollback disponível.

O manifesto é asset **update.json** da Release oficial, com esquema/protocolo 1, canal, versão, build, tag, commit, notas, URLs, tamanhos e SHA-256 do NRO/auxiliar. Tag, commit alvo, tamanho/URL dos assets e manifesto precisam concordar. URLs de binários são restritas ao repositório oficial. Redirects só seguem HTTPS em GitHub/servidor de assets oficial, com limite; não se envia Authorization. Artefatos do Actions não são usados pelo app: seu download requer autenticação GitHub. Não há token pessoal embutido.

Checagem ao abrir tem intervalo mínimo de 24 h; manual, 15 min, persistido antes de iniciar rede para evitar repetição após erro/limite. HTTP 403/429 informa limite e não dispara loop automático. Falha de rede não remove credenciais ou muda canal. Relógio incorreto pode bloquear o intervalo ou a verificação de validade do certificado; não se ignora TLS para contornar isso.

## Confiança e integridade

SSL nativo verifica cadeia de CA, nome do host e data, usando operações não bloqueantes com 15 s para handshake, 10 s para conexão TCP e 5 s por E/S. Erros TLS são propagados. HTTP tem limites de tamanho/cabeçalho/redirect; aceita Content-Length e chunked válidos e recusa respostas ambíguas, parciais e codificações inesperadas. DNS usa os prazos do SDK e não é cancelável por este módulo.

SHA-256 é calculado incrementalmente em buffers limitados e comparado com o manifesto. NRO/segmentos/ASET/NACP/título/versão/build também são conferidos. O auxiliar tem título próprio e o mesmo build dos fontes. O empacotamento usa lista fechada e não inclui configurações do SD.

**Não há assinatura criptográfica independente do manifesto.** A origem é autenticada por HTTPS e pela confiança na conta, repositório e workflows GitHub. Checksum detecta corrupção/troca do binário em relação ao manifesto; não protege contra comprometimento da conta/repositório/CA ou de um mantenedor autorizado que troque ambos. Assinatura com chave fora do GitHub foi avaliada como reforço futuro: exige provisionamento, rotação e recuperação de uma chave independente. Nenhuma chave privada foi solicitada ou criada para aparentar essa garantia. Um atacante que já modifica o SD pode substituir o aplicativo; o updater não é uma fronteira de segurança contra acesso local malicioso.

## Caminho e transação

Alvo único autorizado:

```text
sdmc:/switch/Switch File Manager/SwitchFileManager.nro
```

O argv[0] precisa ser exatamente esse caminho e o arquivo no SD deve ter NACP/build iguais ao executável em execução. Ausência de caminho, cópia em outro local ou divergência bloqueiam instalação, sem escolher outra cópia por aproximação. O diagnóstico de cópias continua somente leitura.

Cada tarefa cria `.sfm-update-N/` exclusiva na pasta oficial, contendo:

- `owner.txt`: marcador do protocolo.
- `new.nro` e `helper.nro`: downloads separados e verificados.
- `transaction.json`: registro imutável com versão/build/hashes e identidade anterior.
- `old.nro`: backup do principal após o auxiliar iniciar a troca.
- `rollback.request` e `reverted.nro`: pedido explícito e NRO novo preservado após rollback, quando aplicável.

O auxiliar é carregado de sua área exclusiva; não troca o arquivo que está executando. O [nx-hbloader pesquisado](https://github.com/switchbrew/nx-hbloader/blob/82b95122c5ae8dc059bf23893ba7623c72c86773/source/main.c) lê o NRO, fecha seu descritor e mapeia seus segmentos antes do salto para o programa. A API [envHasNextLoad/envSetNextLoad](https://github.com/switchbrew/libnx/blob/7644c9b26099aa2d2145bc72a21ee24190e92085/nx/include/switch/runtime/env.h) detecta suporte do launcher. Isso fundamenta o projeto do auxiliar, mas não comprova funcionamento em toda versão do Atmosphère/launcher: a passagem e retorno ainda precisam do teste físico.

Antes de baixar, statvfs verifica NRO + auxiliar + margem de 1 MiB; backup usa rename no mesmo volume, sem duplicar bytes do NRO anterior. No libnx, f_frsize/f_bsize são 1 e f_bavail já contém bytes. Não há reserva exclusiva de espaço: outro escritor pode consumi-lo depois. Erros de escrita/sync/close são tratados.

O auxiliar revalida journal, arquivos e hash anterior; move o alvo para nome livre `old.nro`, confere backup e publica `new.nro` em alvo vazio. No libnx, rename não sobrescreve destino existente; testes impõem essa semântica no host. Repetição reconhece o hash já instalado sem publicar novamente. Falha simulada ao publicar restaura o anterior; após interrupção abrupta, journal/backup permitem repetir a operação. `fsync`/rename não garantem atomicidade contra corte elétrico, corrupção do controlador ou alterações externas simultâneas.

## Falhas e recuperação

- Download parcial/checksum errado: principal permanece intacto, tarefa não fica pronta; nova tentativa cria outra área. Não há retomada Range do download do atualizador nesta versão.
- Caminho/build errado, downgrade, canal incompatível ou launcher sem next-load: instalação bloqueada.
- Falha antes de backup: NRO anterior permanece no alvo.
- Interrupção entre backup e publicação: auxiliar pode continuar a partir do journal ou restaurar backup; não se publica parcial.
- Interrupção após publicação: reexecução reconhece hash novo e é idempotente.
- Rollback interrompido: `old.nro` e `reverted.nro` preservam as versões; reexecução termina a restauração sem sobrescrever arquivos desconhecidos.
- Se o alvo ficou ausente por interrupção real, o diagnóstico/recuperação usa o auxiliar da área da tarefa ou cópia manual do backup verificado, pelo Android. Não formatar nem apagar configurações para reparar o NRO.
- Se o launcher recusar retorno após instalação, o auxiliar mostra o código; abrir o caminho oficial pelo Homebrew Menu. O backup continua preservado.

Jobs/backups não são expurgados automaticamente. Só a transação mais recente registrada é oferecida pela UI; áreas antigas permanecem para diagnóstico. Um pedido persistente de rollback não deve ser confundido com uma nova instalação: baixar novamente cria outra tarefa. Não editar os mesmos arquivos internos por FTP enquanto o atualizador trabalha. Configurações, logs, favoritos/histórico e OAuth legados não são removidos.

Preferências ficam em `sdmc:/switch/Switch File Manager/update-settings.json`, usando gravação/releitura com backups do armazenamento existente, separadas dos dados Drive. Configuração inválida é preservada e bloqueia checagem automática. Consulte [testes](TESTES.md), [promoção](PROMOCAO_E_RELEASES.md) e [checklist físico](VALIDACAO_SWITCH_LITE_3_3.md).
