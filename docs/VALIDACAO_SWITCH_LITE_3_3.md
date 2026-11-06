# Checklist físico — atualizador 0.10.13

Nenhum item abaixo foi executado no Switch nesta sessão. Não marcar aprovação por CI. Usar somente arquivos/conta de teste e espaço disponível; não provocar corte de energia, SD cheio/corrupção ou exclusão em dados pessoais. Essas falhas são simuladas no host.

Registrar versão/build/SHA-256 do NRO e auxiliar, caminho argv[0] exibido, Atmosphère/firmware, launcher, applet/memória completa e FAT32/exFAT quando conhecidos. O artefato instalado e o manifesto remoto de teste precisam corresponder aos hashes registrados. Uma atualização exige **versão superior** publicada em pré-Release oficial compatível; não reutilizar número nem adulterar o manifesto para forçar instalação.

1. Instalar pacote Android no caminho oficial e conferir versão/build/NACP e marca preservada. **[ ] Funcionou / [ ] Não funcionou**
2. Abrir Atualizações por controles e toque; padrão ESTÁVEL e checagem ao abrir ativada, com UI utilizável durante consulta. **[ ] Funcionou / [ ] Não funcionou**
3. Desativar checagem, reabrir e conferir persistência; consulta manual funciona respeitando intervalo. **[ ] Funcionou / [ ] Não funcionou**
4. Mudar para EXPERIMENTAL após aviso, conferir canal/versão/notas/tamanho e ausência de oferta experimental no estável. **[ ] Funcionou / [ ] Não funcionou**
5. Sem Wi-Fi/retorno de rede: erro técnico visível, UI/FTP/Drive preservados e nova consulta respeitando limite. **[ ] Funcionou / [ ] Não funcionou**
6. Baixar candidata mais nova, confirmar bytes/estado e continuar usando UI durante worker; não instalar automaticamente. **[ ] Funcionou / [ ] Não funcionou**
7. Interromper internet durante download: instalado permanece íntegro; parcial é preservado e nova tentativa cria outra área. **[ ] Funcionou / [ ] Não funcionou**
8. Cancelar tarefa no menu: worker termina, sem alterar o instalado nem arquivos pessoais. **[ ] Funcionou / [ ] Não funcionou**
9. Confirmar instalação: aplicativo encerra serviços, auxiliar abre, valida arquivos, cria backup e instala o alvo correto. **[ ] Funcionou / [ ] Não funcionou**
10. Retorno automático ao aplicativo novo: conferir versão/build/caminho e hash, não apenas nome da pasta. **[ ] Funcionou / [ ] Não funcionou**
11. Recuperar versão anterior mediante confirmação: backup validado restaura, novo NRO fica preservado e aplicativo antigo reabre. **[ ] Funcionou / [ ] Não funcionou**
12. Executar cópia de teste fora do caminho oficial: instalação é bloqueada, nenhuma cópia é apagada. **[ ] Funcionou / [ ] Não funcionou**
13. Abrir no launcher/modo sem next-load, se disponível: instalação bloqueada e diagnóstico claro. **[ ] Funcionou / [ ] Não funcionou**
14. Testar FTP isolado/sequencial e FTP + Drive enquanto ocorre somente consulta/download do atualizador, com arquivos diferentes. **[ ] Funcionou / [ ] Não funcionou**
15. Revalidar Drive, Manter conectado, desconectar/reconectar, retomada Drive, ZIP, dois painéis, toque e favoritos/histórico. **[ ] Funcionou / [ ] Não funcionou**
16. Conferir configurações OAuth legadas e arquivos pessoais preservados após instalação/rollback. **[ ] Funcionou / [ ] Não funcionou**

Deixar ambos vazios se não executado, com justificativa. Não compartilhar tokens, credenciais ou o diretório de dados. Erros devem registrar etapa/código, estado, canal e identidade dos binários. Aprovação estável só depois de todos os testes aplicáveis aprovados e registro estruturado para o commit exato, conforme [promoção](PROMOCAO_E_RELEASES.md). A primeira promoção continua bloqueada.
