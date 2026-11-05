#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace sfm::storage {
bool pathExists(const std::string& path);
bool isDir(const std::string& path);
std::string uniqueName(const std::string& path);
std::vector<std::string> listNames(const std::string& path);
uint64_t treeSize(const std::string& path);
}
