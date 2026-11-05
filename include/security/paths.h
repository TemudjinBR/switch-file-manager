#pragma once
#include <string>
#include <vector>
namespace sfm::security {
bool isCritical(std::string path);
std::string safeName(std::string name);
bool validName(const std::string& name);
bool safeComponents(const std::string& raw, std::vector<std::string>& out);
}
