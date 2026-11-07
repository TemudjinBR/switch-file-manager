#pragma once
#include <string>
namespace sfm::ui {
struct AboutInfo {
  std::string version, build, updateChannel;
};
void drawAbout(const AboutInfo &info);
} // namespace sfm::ui
