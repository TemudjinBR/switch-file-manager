#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace gd::downloads {
enum class State { Connecting, Downloading, Retrying, Paused, Restarting, Validating, Finalizing, Completed, Cancelled, Failed };
enum class Action { Continue, Pause, Cancel };
enum class Kind { Network, Storage, Protocol, Authorization };
struct Error { Kind kind = Kind::Protocol; std::string stage; int code = 0; std::string message; };
struct Remote { std::string id, version, mediaTag; uint64_t size = 0; };
struct Status {
    State state = State::Connecting;
    uint64_t current = 0, total = 0, networkBytes = 0;
    unsigned attempt = 1, maxAttempts = 6, delayMs = 0;
    std::string reason;
};
struct Observer {
    virtual ~Observer() = default;
    virtual bool bytes(uint64_t) { return true; }
    virtual Action action() { return Action::Continue; }
    virtual void update(const Status&) {}
};
struct Fetch {
    int http = 0;
    bool complete = false, headersValid = false, retryable = true, restart = false;
    std::string mediaTag;
};
using Writer = std::function<bool(const uint8_t*, size_t)>;
struct Transport {
    virtual ~Transport() = default;
    virtual bool inspect(const std::string& id, Remote& remote, Error& error) = 0;
    virtual Fetch fetch(const Remote& remote, uint64_t first, uint64_t last, const Writer& write, Error& error) = 0;
};
struct Policy { unsigned maxRetries = 5, baseDelayMs = 1000, maxDelayMs = 10000; uint64_t chunkBytes = 8u << 20; };
struct Job {
    std::string key, owner, id, destination;
    Remote remote;
    uint64_t offset = 0, generation = 0;
    uint32_t checksum = 0;
    State state = State::Connecting;
};
const char* label(State state);
bool run(const std::string& area, const std::string& owner, const std::string& id,
         const std::string& destination, Transport& transport, Observer* observer, Error& error, Policy policy = {});
bool list(const std::string& area, const std::string& owner, std::vector<Job>& jobs, unsigned& invalid, Error& error);
bool cancel(const std::string& area, const std::string& owner, const Job& job, Error& error);
#ifdef HOST_TEST
void testFailure(const std::string& stage, int code);
void testCrash(const std::string& stage);
#endif
}
