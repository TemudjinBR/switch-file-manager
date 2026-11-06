#pragma once
#include <cstdint>
#include <string>
namespace sfm::storage {
struct Progress {
    virtual bool cancelled() const = 0;
    virtual void update(const std::string& name, uint64_t bytes, bool completed) = 0;
    virtual ~Progress() = default;
};
// A cópia cria um destino exclusivo e tenta limpar somente o que criou em caso de falha.
bool copyTree(const std::string& source, const std::string& target, Progress* progress, std::string& error);
bool verifyTreeCopy(const std::string& source, const std::string& target, Progress* progress, std::string& error);
bool removeTree(const std::string& path, Progress* progress, std::string& error);
}
