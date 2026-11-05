# Pacote de preparação v0.10.8

Este ZIP **não é um NRO compilado**. Contém arquivos para o repositório GitHub.

1. Adicione `apply_log_fix_0108.py` à raiz do repositório.
2. No `Makefile`, altere `APP_VERSION := 0.10.7` para `APP_VERSION := 0.10.8`.
3. Logo após `@python3 $(CURDIR)/apply_diagnostics.py`, acrescente:
   `@python3 $(CURDIR)/apply_log_fix_0108.py`
4. Substitua `.github/workflows/build.yml` pelo arquivo do pacote.
5. Adicione `release-notes/v0.10.8.md`.
6. Faça commit na branch `main`. A Action deverá compilar e publicar v0.10.8 com descrição própria, **desde que a tag ainda não exista**.

**Atenção:** a integração GitHub desta conversa retornou 403 para escrita; nada deste pacote foi aplicado remotamente. Testes físicos e compilação no GitHub ainda são necessários. Não exclua pastas ou logs antigos.
