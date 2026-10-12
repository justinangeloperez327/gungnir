#pragma once

// Implementation details: parsed templates are immutable; render state is local.
#include <gungnir/view/engine.hpp>
#include <gungnir/view/error.hpp>
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <vector>

namespace gungnir::view::detail {

struct ValueAccess {
    static const Value::Storage& storage(const Value& value) noexcept { return value.storage_; }
};

inline constexpr std::size_t source_limit = 2 * 1024 * 1024;
inline constexpr std::size_t output_limit = 8 * 1024 * 1024;
struct Location { std::size_t line{1}, column{1}; };
struct Expression;
using Expr = std::shared_ptr<const Expression>;
struct Expression {
    enum class Kind { literal, variable, member, index, helper, unary, binary, conditional, array, object };
    Kind kind{Kind::literal};
    Location location;
    String text;
    Value value;
    std::vector<Expr> children;
    std::vector<String> keys;
    unsigned height{1};
};

struct Node {
    enum class Kind { text, escaped, raw, condition, loop, include, section, yield, extends };
    Kind kind{Kind::text};
    Location location;
    String text, key, variable;
    std::vector<Expr> expressions;
    std::vector<Node> children, otherwise;
    std::vector<std::vector<Node>> branches;
};
struct Template {
    String name, source;
    bool spear{false};
    std::vector<Node> nodes;
    std::size_t cache_cost{0};
};

[[nodiscard]] std::shared_ptr<const Template> compile(String name, String source, bool spear);
using Sections = std::unordered_map<String, String>;
class Session {
public:
    explicit Session(const Engine& owner) : engine(owner), root_(owner.root()) {}
    [[nodiscard]] String render(std::string_view name, const Data& data, const Value* current,
                                Sections& sections, unsigned depth);
    [[nodiscard]] String spear(const Template& file, const Data& data, Sections& sections, unsigned depth);
    void append(String& output, std::string_view value);
    void step();
    const Engine& engine;
private:
    [[nodiscard]] Value evaluate(const Template& file, const Expr& expression, const Data& data, unsigned depth = 0);
    [[nodiscard]] String nodes(const Template& file, const std::vector<Node>& nodes, const Data& data,
                               Sections& sections, unsigned depth);
    std::filesystem::path root_;
    std::vector<String> stack_;
    std::size_t remaining_{output_limit}, operations_{1000000};
};

[[nodiscard]] String render_legacy(std::string_view source, const Data& data, const Value* current,
                                  Session& session, Sections& sections, unsigned depth);
[[nodiscard]] String escape(std::string_view source);
[[noreturn]] inline void fail(const Template& file, Location location, String message) {
    throw SyntaxError{std::move(message), file.name, location.line, location.column};
}

} // namespace gungnir::view::detail
