#pragma once
#include <string>
#include <functional>
#ifdef HOST_TEST
#include <vector>
#endif

namespace gd::persist {
enum class File { Credentials, Token, LoginState, Account, Transfer, Updates };
struct Error {
    std::string stage;
    int code = 0;
    std::string recoveryStage;
    int recoveryCode = 0;
};
// Gravações mantêm caminhos legados e preservam backups. Exclusão exige eraseMatching explícito.
bool ensureDirectory(const std::string& directory, Error& error);
bool load(const std::string& directory, File file, std::string& data, Error& error, const char* phase = "legado");
bool save(const std::string& directory, File file, const std::string& data, Error& error);
enum class EraseDecision { Preserve, Remove, Unknown };
// Somente para a ação explícita Esquecer. O predicado identifica a conta pelo conteúdo.
bool eraseMatching(const std::string& directory, File file,
                   const std::function<EraseDecision(const std::string&)>& decide,
                   unsigned& unknown, Error& error);
const char* filename(File file);
std::string describe(const Error& error);

#ifdef HOST_TEST
struct Fault { std::string stage; int code; int remaining = 1; int skip = 0; };
void testFaults(std::vector<Fault> faults);
void testWriteLimit(unsigned bytes);
void testCrashAt(const std::string& stage);
#endif
}
