#pragma once
#include <filesystem>
#include <cctype>
#include <fstream>
#include <functional>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gungnir::language {

struct Import {
    std::string module;
    std::optional<std::string> alias;
};

struct Module {
    std::string name;
    std::filesystem::path path;
    std::vector<Import> imports;
};

class ModuleResolver {
public:
    explicit ModuleResolver(std::filesystem::path root) : root_(std::move(root)) {}

    [[nodiscard]] std::filesystem::path resolve(std::string_view name) const {
        std::filesystem::path relative;
        std::string part;
        const auto push = [&] {
            if (part.empty() || (!std::isalpha(static_cast<unsigned char>(part.front())) && part.front() != '_'))
                throw std::invalid_argument("Invalid module name: " + std::string{name});
            relative /= part; part.clear();
        };
        for (const unsigned char c : name) {
            if (c == '.') push();
            else if (std::isalnum(c) || c == '_') part += static_cast<char>(c);
            else throw std::invalid_argument("Invalid module name: " + std::string{name});
        }
        push(); relative += ".gnr";
        const auto base = std::filesystem::weakly_canonical(root_);
        const auto path = std::filesystem::weakly_canonical(base / relative);
        const auto within = path.lexically_relative(base);
        if (within.empty() || *within.begin() == "..") throw std::invalid_argument("Module resolves outside project root");
        if (!std::filesystem::is_regular_file(path)) throw std::runtime_error("Gungnir module not found: " + std::string{name});

        return path;
    }

private:
    std::filesystem::path root_;
};

class IncrementalBuildCache {
public:
    [[nodiscard]] bool changed(const std::filesystem::path& path) {
        std::ifstream input{path, std::ios::binary};
        if (!input) throw std::runtime_error("Unable to read module: " + path.string());
        const std::string content{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
        const auto hash = std::hash<std::string>{}(content);
        const auto key = path.generic_string();
        const auto found = hashes_.find(key);
        if (found != hashes_.end() && found->second == hash) return false;
        hashes_.insert_or_assign(key, hash);
        return true;
    }
    void clear() { hashes_.clear(); }

private:
    std::unordered_map<std::string, std::size_t> hashes_;
};

} // namespace gungnir::language
