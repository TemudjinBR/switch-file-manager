#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
namespace sfm::updates {
struct Response {
    int status = 0;
    std::map<std::string, std::string> headers;
    uint64_t bytes = 0;
};
using Sink = std::function<bool(const uint8_t *, size_t)>;
bool getHttps(const std::string &url, uint64_t limit, const Sink &sink, std::atomic<bool> &cancel,
              Response &response, std::string &error);
#ifdef HOST_TEST
extern int httpTestPort;
#endif
} // namespace sfm::updates
