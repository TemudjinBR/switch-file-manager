#pragma once
#include <string>
#ifdef HOST_TEST
#include <vector>
#endif

namespace gd::persist {
enum class File { Credentials, Token, LoginState };
struct Error {
    std::string stage;
    int code = 0;
    std::string recoveryStage;
    int recoveryCode = 0;
};
// Mantém caminhos legados. Nunca remove o arquivo canônico nem backups de dados.
bool ensureDirectory(const std::string& directory, Error& error);
bool load(const std::string& directory, File file, std::string& data, Error& error);
bool save(const std::string& directory, File file, const std::string& data, Error& error);
const char* filename(File file);
std::string describe(const Error& error);

#ifdef HOST_TEST
struct Fault { std::string stage; int code; int remaining = 1; int skip = 0; };
void testFaults(std::vector<Fault> faults);
void testWriteLimit(unsigned bytes);
void testCrashAt(const std::string& stage);
#endif
}
