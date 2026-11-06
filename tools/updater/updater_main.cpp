#include "updates/update_core.h"
#include "version.h"
#include <cstdio>
#include <string>
#include <switch.h>
// O auxiliar e outro NRO: o aplicativo antigo ja saiu antes de mover seu arquivo.
static const char identity[] = "SFM_BUILD:" SFM_BUILD_ID;
int main(int argc, char **argv) {
    consoleInit(nullptr);
    printf("Atualizador do Switch File Manager %s\n%s\n", SFM_VERSION, identity);
    std::string path = argc > 0 && argv && argv[0] ? argv[0] : "";
    const std::string suffix = "/helper.nro";
    std::string error;
    sfm::updates::Transaction transaction;
    bool ok = false;
    if (path.size() > suffix.size() && path.substr(path.size() - suffix.size()) == suffix &&
        envHasNextLoad()) {
        std::string dir = path.substr(0, path.size() - suffix.size());
        if (sfm::updates::readTransaction(dir, transaction, error)) {
            std::string request;
            std::string ignored;
            bool rollback = sfm::updates::readSmall(dir + "/rollback.request", request, ignored) &&
                            request == "SFM_ROLLBACK_1\n";
            ok = sfm::updates::install(transaction, rollback, error);
            if (ok) {
                printf("Instalacao/recuperacao verificada. Reabrindo aplicativo...\n");
                Result rc = envSetNextLoad(sfm::updates::officialTarget,
                                           (std::string("\"") + sfm::updates::officialTarget + "\"").c_str());
                if (R_SUCCEEDED(rc)) {
                    consoleExit(nullptr);
                    return 0;
                }
                error = "Instalado, mas retorno ao aplicativo falhou: " + std::to_string(unsigned(rc));
            }
        }
    } else
        error = "Auxiliar sem caminho/next-load seguro; nenhum NRO foi alterado";
    printf("%s\n\nNROs/backups preservados. Consulte o guia de recuperacao.\nB: sair\n", error.c_str());
    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_B)
            break;
        consoleUpdate(nullptr);
    }
    consoleExit(nullptr);
    return ok ? 0 : 1;
}
