#pragma once
#include <string>
namespace sfm_diag {
// Retorna false em falhas de diretório, rotação ou gravação; não lança exceções.
bool write(const char* category, const std::string& message);
int lastErrorCode();
}
