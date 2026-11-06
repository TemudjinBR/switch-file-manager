#pragma once
#include <string>
namespace sfm::oauth {
// Aleatoriedade do sistema e PKCE S256, sem logs/conteúdo persistente.
bool randomToken(std::string& out);
std::string challenge(const std::string& verifier);
}
