#pragma once

#include <filesystem>
#include <cstddef>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <shared_mutex>
#include <string_view>

#include <gungnir/core/types.hpp>
#include <gungnir/view/data.hpp>

namespace gungnir::view {

namespace detail { struct Template; class Session; }

enum class CacheMode { reload, immutable };

class Engine {
public:
    using Helper = std::function<Value(const std::vector<Value>&)>;
    Engine& helper(String name, Helper helper);
    [[nodiscard]] String source(std::string_view name) const;
    [[nodiscard]] Value call(std::string_view name, const std::vector<Value>& arguments) const;
    explicit Engine(std::filesystem::path root = "views");

    Engine& root(std::filesystem::path value);
    [[nodiscard]]
    std::filesystem::path root()
        const;

    [[nodiscard]] String render(
        std::string_view name,
        const Data& data = {}
    ) const;

    [[nodiscard]] String render_text(
        std::string_view source,
        const Data& data = {}
    ) const;

    // Inline legacy templates keep their existing grammar. Spear is explicit.
    [[nodiscard]] String render_spear_text(
        std::string_view source,
        const Data& data = {},
        std::string_view name = "<inline.spear>"
    ) const;

    Engine& cache_mode(CacheMode mode);
    [[nodiscard]] CacheMode cache_mode() const;
    void clear_cache();
    [[nodiscard]] std::size_t cached_templates() const;

private:
    friend class detail::Session;
    [[nodiscard]] std::shared_ptr<const detail::Template> load(
        const std::filesystem::path& root, std::string_view name
    ) const;
    mutable std::shared_mutex mutex_;
    std::filesystem::path root_;
    std::unordered_map<String, Helper> helpers_;
    CacheMode cache_mode_{CacheMode::reload};
    mutable std::unordered_map<String, std::shared_ptr<const detail::Template>> cache_;
    mutable std::size_t cached_bytes_{0};
};

} // namespace gungnir::view
