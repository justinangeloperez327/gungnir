#pragma once
#include <algorithm>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace gungnir::language {

class DependencyGraph {
public:
    void add(std::string module, std::vector<std::string> dependencies) {
        graph_.insert_or_assign(std::move(module), std::move(dependencies));
    }

    [[nodiscard]] std::vector<std::string> order() const {
        std::vector<std::string> result;
        std::unordered_set<std::string> visiting;
        std::unordered_set<std::string> visited;
        std::vector<std::string> modules;
        for (const auto& [module, _] : graph_) modules.push_back(module);
        std::sort(modules.begin(),modules.end());
        for (const auto& module : modules) visit(module, visiting, visited, result);
        return result;
    }

private:
    void visit(const std::string& module, std::unordered_set<std::string>& visiting,
               std::unordered_set<std::string>& visited, std::vector<std::string>& result) const {
        if (visited.contains(module)) return;
        if (!visiting.insert(module).second) throw std::runtime_error("Circular Gungnir module dependency: " + module);
        if (const auto found = graph_.find(module); found != graph_.end()) {
            auto dependencies = found->second;
            std::sort(dependencies.begin(),dependencies.end());
            for (const auto& dependency : dependencies) visit(dependency, visiting, visited, result);
        } else throw std::runtime_error("Missing Gungnir module dependency: " + module);
        visiting.erase(module);
        visited.insert(module);
        result.push_back(module);
    }

    std::unordered_map<std::string, std::vector<std::string>> graph_;
};

} // namespace gungnir::language
