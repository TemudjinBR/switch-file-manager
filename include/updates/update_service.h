#pragma once
#include "updates/update_core.h"
#include <atomic>
#include <mutex>
#include <pthread.h>
namespace sfm::updates {
struct Preferences {
    std::string channel = "stable", pending;
    bool automatic = true;
    uint64_t lastCheck = 0;
};
struct Snapshot {
    std::string state = "Aguardando", error;
    Manifest available;
    uint64_t done = 0, total = 0;
    bool busy = false, ready = false;
};
class Service {
    mutable std::mutex mutex;
    Preferences preferences;
    Snapshot current;
    std::atomic<bool> cancelled{false};
    pthread_t worker{};
    bool joinable = false;
    std::string executable, installed, identity;
    bool store(std::string &error);
    bool launch(bool download, std::string &error);
    void work(bool download);

  public:
    ~Service();
    void initialize(std::string executable, std::string version, std::string build);
    Preferences prefs() const;
    Snapshot status() const;
    bool configure(const std::string &channel, bool automatic, std::string &error);
    bool check(bool startup, std::string &error);
    bool download(std::string &error);
    void cancel();
    void stop();
    bool pending(Transaction &transaction, std::string &error) const;
};
} // namespace sfm::updates
