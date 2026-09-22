#pragma once

#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace gungnir::language {

struct Module {
    std::string name;
    std::filesystem::path path;
    std::vector<std::string> imports;
};

class ModuleResolver {
public:
    explicit ModuleResolver(std::filesystem::path root):root_(std::move(root)){}
    [[nodiscard]] std::filesystem::path resolve(std::string_view name) const {
        std::filesystem::path relative;
        std::string part;
        for(char c:name){if(c=='.'){relative/=part;part.clear();}else part+=c;}
        relative/=part+".gnr";
        const auto path=(root_/relative).lexically_normal();
        if(!std::filesystem::exists(path))throw std::runtime_error("Gungnir module not found: "+std::string{name});
        return path;
    }
private:
    std::filesystem::path root_;
};

class IncrementalBuildCache {
public:
    [[nodiscard]] bool changed(const std::filesystem::path& path) {
        std::ifstream input{path,std::ios::binary};
        const std::string content{std::istreambuf_iterator<char>{input},std::istreambuf_iterator<char>{}};
        const auto hash=std::hash<std::string>{}(content);
        const auto key=path.generic_string();
        const auto found=hashes_.find(key);
        if(found!=hashes_.end()&&found->second==hash)return false;
        hashes_.insert_or_assign(key,hash); return true;
    }
    void clear(){hashes_.clear();}
private:
    std::unordered_map<std::string,std::size_t> hashes_;
};

} // namespace gungnir::language
