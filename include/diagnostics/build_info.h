#pragma once
#include <functional>
#include <string>
#include <vector>
namespace sfm::build {
const char* identity();
struct Copy { std::string path, version, build; };
struct Scan { std::vector<Copy> copies; unsigned examined = 0; bool limited = false, cancelled = false; };
// Somente leitura; limites de profundidade, entradas, bytes e copias. Nunca remove NROs.
Scan inspect(const std::string& root, const std::function<bool()>& keepGoing = {});
}
