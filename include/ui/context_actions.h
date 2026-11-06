#pragma once
// Modelo puro do menu contextual. Independente do libnx; testável no host.
#include <cstddef>
#include <vector>

namespace sfm::ui::actions {
enum class Command { Copy, Move, SelectAll, ClearAll, Delete, Rename, CompressZip, ExtractZip };
struct Context {
    bool focused = false;      // foco atribuído explicitamente por toque/controle
    std::size_t selected = 0;   // número de arquivos/pastas efetivamente selecionados
    std::size_t marked = 0;     // marcações explícitas do painel ativo
    bool allZip = false;        // todos são arquivos ZIP (não diretórios)
    bool cloud = false;         // ZIP local é indisponível no Drive
};

inline std::vector<Command> contextualCommands(const Context& context) {
    if ((!context.focused && context.marked == 0) || context.selected == 0)
        return {}; // menu geral
    std::vector<Command> commands = {
        Command::Copy,
        Command::Move,
        context.marked > 1 ? Command::ClearAll : Command::SelectAll,
        Command::Delete
    };
    if (context.selected == 1)
        commands.push_back(Command::Rename);
    if (!context.cloud)
        commands.push_back(context.allZip ? Command::ExtractZip : Command::CompressZip);
    return commands;
}
} // namespace sfm::ui::actions
