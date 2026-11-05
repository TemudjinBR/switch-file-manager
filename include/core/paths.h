#pragma once
#include <string>
namespace sfm::paths {
inline constexpr char kLegacyConfigDir[] = "sdmc:/switch/switch-manager";
inline constexpr char kLogDirectory[] = "sdmc:/switch/Switch File Manager/logs";
inline constexpr char kLogFile[] = "sdmc:/switch/Switch File Manager/logs/sfm.log";
std::string join(const std::string& a, const std::string& b);
std::string baseName(std::string path);
std::string parentOf(std::string path);
}
