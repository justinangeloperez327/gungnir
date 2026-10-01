#include <gungnir/view/engine.hpp>
#include <gungnir/view/error.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>

namespace gungnir::view {

namespace {

[[nodiscard]]
bool is_within(
    const std::filesystem::path& root,
    const std::filesystem::path& candidate
) {
    auto root_part =
        root.begin();

    auto candidate_part =
        candidate.begin();

    while (
        root_part != root.end()
    ) {
        if (
            candidate_part ==
                candidate.end() ||
            *candidate_part !=
                *root_part
        ) {
            return false;
        }

        ++root_part;
        ++candidate_part;
    }

    return true;
}

[[nodiscard]]
std::filesystem::path normalize_root(
    std::filesystem::path value
) {
    if (value.empty()) {
        throw InvalidPath{
            value.generic_string()
        };
    }

    std::error_code error;

    auto absolute =
        std::filesystem::absolute(
            std::move(value),
            error
        );

    if (error) {
        throw Error{
            "Unable to resolve Gungnir view root: " +
            error.message()
        };
    }

    absolute =
        absolute.lexically_normal();

    const auto status =
        std::filesystem::symlink_status(
            absolute,
            error
        );

    if (
        !error &&
        std::filesystem::is_symlink(
            status
        )
    ) {
        throw InvalidPath{
            absolute.generic_string()
        };
    }

    return absolute;
}

[[nodiscard]]
std::filesystem::path resolve_template(
    const std::filesystem::path& root,
    std::string_view name
) {
    if (
        name.empty() ||
        name.find('\0') !=
            std::string_view::npos
    ) {
        throw InvalidPath{
            std::string{name}
        };
    }

    std::filesystem::path relative{
        std::string{name}
    };

    if (
        relative.is_absolute() ||
        relative.has_root_name() ||
        relative.has_root_directory()
    ) {
        throw InvalidPath{
            std::string{name}
        };
    }

    for (const auto& part : relative) {
        if (part == "..") {
            throw InvalidPath{
                std::string{name}
            };
        }
    }

    relative =
        relative.lexically_normal();

    if (
        relative.empty() ||
        relative == "."
    ) {
        throw InvalidPath{
            std::string{name}
        };
    }

    if (!relative.has_extension()) {
        relative += ".html";
    }

    std::error_code error;

    const auto root_status =
        std::filesystem::symlink_status(
            root,
            error
        );

    if (error) {
        if (
            error ==
            std::errc::
                no_such_file_or_directory
        ) {
            throw NotFound{
                root.generic_string()
            };
        }

        throw Error{
            "Unable to inspect Gungnir view root: " +
            error.message()
        };
    }

    if (
        std::filesystem::is_symlink(
            root_status
        )
    ) {
        throw InvalidPath{
            root.generic_string()
        };
    }

    const auto canonical_root =
        std::filesystem::weakly_canonical(
            root,
            error
        );

    if (error) {
        throw Error{
            "Unable to canonicalize Gungnir view root: " +
            error.message()
        };
    }

    auto current = root;

    for (const auto& part : relative) {
        if (
            part.empty() ||
            part == "."
        ) {
            continue;
        }

        current /= part;

        error.clear();

        const auto status =
            std::filesystem::symlink_status(
                current,
                error
            );

        if (error) {
            if (
                error ==
                std::errc::
                    no_such_file_or_directory
            ) {
                break;
            }

            throw Error{
                "Unable to inspect Gungnir view path: " +
                error.message()
            };
        }

        if (
            std::filesystem::is_symlink(
                status
            )
        ) {
            throw InvalidPath{
                std::string{name}
            };
        }
    }

    error.clear();

    const auto candidate =
        std::filesystem::weakly_canonical(
            root / relative,
            error
        );

    if (error) {
        throw Error{
            "Unable to resolve Gungnir view path: " +
            error.message()
        };
    }

    if (
        !is_within(
            canonical_root,
            candidate
        )
    ) {
        throw InvalidPath{
            std::string{name}
        };
    }

    error.clear();

    const auto final_status =
        std::filesystem::symlink_status(
            candidate,
            error
        );

    if (
        !error &&
        std::filesystem::is_symlink(
            final_status
        )
    ) {
        throw InvalidPath{
            std::string{name}
        };
    }

    return candidate;
}

std::string_view trim(std::string_view value) {
    while (
        !value.empty() &&
        std::isspace(static_cast<unsigned char>(value.front())) != 0
    ) {
        value.remove_prefix(1);
    }

    while (
        !value.empty() &&
        std::isspace(static_cast<unsigned char>(value.back())) != 0
    ) {
        value.remove_suffix(1);
    }

    return value;
}

String escape_html(std::string_view value) {
    String result;
    result.reserve(value.size());

    for (const char character : value) {
        switch (character) {
        case '&':
            result += "&amp;";
            break;
        case '<':
            result += "&lt;";
            break;
        case '>':
            result += "&gt;";
            break;
        case '"':
            result += "&quot;";
            break;
        case '\'':
            result += "&#39;";
            break;
        default:
            result.push_back(character);
            break;
        }
    }

    return result;
}

const Value* resolve_path(
    std::string_view path,
    const Data& data,
    const Value* current
) {
    path = trim(path);

    if (path == "this") {
        return current;
    }

    auto resolve_from = [&](const Value* root) -> const Value* {
        if (!root) {
            return nullptr;
        }

        const Value* value = root;
        std::size_t start = 0;

        while (start <= path.size()) {
            const auto separator = path.find('.', start);
            const auto part = separator == std::string_view::npos
                ? path.substr(start)
                : path.substr(start, separator - start);

            if (part.empty()) {
                return nullptr;
            }

            value = value->get(part);
            if (!value) {
                return nullptr;
            }

            if (separator == std::string_view::npos) {
                return value;
            }

            start = separator + 1;
        }

        return value;
    };

    if (current && current->is_object()) {
        if (const auto* found = resolve_from(current)) {
            return found;
        }
    }

    const auto first_separator = path.find('.');
    const auto root_name = first_separator == std::string_view::npos
        ? path
        : path.substr(0, first_separator);

    const auto* root = data.get(root_name);
    if (!root) {
        return nullptr;
    }

    if (first_separator == std::string_view::npos) {
        return root;
    }

    auto remaining = path.substr(first_separator + 1);
    const Value* value = root;

    while (!remaining.empty()) {
        const auto separator = remaining.find('.');
        const auto part = separator == std::string_view::npos
            ? remaining
            : remaining.substr(0, separator);

        value = value->get(part);
        if (!value) {
            return nullptr;
        }

        if (separator == std::string_view::npos) {
            break;
        }

        remaining.remove_prefix(separator + 1);
    }

    return value;
}

struct RenderState {
    const Engine& engine;
    std::unordered_map<String, String> sections;
    std::size_t remaining{8 * 1024 * 1024};
};
std::vector<std::string_view> arguments(std::string_view source, char separator = ',') {
    std::vector<std::string_view> result;
    std::size_t start = 0; char quote = 0; int nested = 0;
    for (std::size_t i = 0; i < source.size(); ++i) {
        char c = source[i];
        if (quote) { if (c == quote && (!i || source[i-1] != '\\')) quote = 0; continue; }
        if (c == '\'' || c == '"') { quote = c; continue; }
        if (c == '(') ++nested;
        if (c == ')') --nested;
        if (nested < 0) throw SyntaxError{"Unbalanced helper arguments"};
        if (c == separator && nested == 0) {
            if (auto part = trim(source.substr(start, i-start)); !part.empty()) result.push_back(part);
            start = i + 1;
        }
    }
    if (quote || nested) throw SyntaxError{"Unclosed helper argument"};
    if (auto part = trim(source.substr(start)); !part.empty()) result.push_back(part);
    return result;
}
Value evaluate(std::string_view expression, const Data& data, const Value* current, RenderState& state, unsigned depth = 0) {
    if (depth > 32) throw SyntaxError{"View expression nesting limit exceeded"};
    expression = trim(expression);
    if (expression.empty()) throw SyntaxError{"Empty view expression"};
    if ((expression.front() == '\'' || expression.front() == '"') && expression.back() == expression.front())
        return String{expression.substr(1, expression.size()-2)};
    if (expression == "true") return true;
    if (expression == "false") return false;
    if (expression == "null") return nullptr;
    if (auto open = expression.find('('); open != std::string_view::npos) {
        if (!expression.ends_with(')')) throw SyntaxError{"Helper call is missing ')'"};
        std::vector<Value> values;
        for (auto item : arguments(expression.substr(open + 1, expression.size() - open - 2))) values.push_back(evaluate(item,data,current,state,depth+1));
        return state.engine.call(trim(expression.substr(0,open)),values);
    }
    if (auto* value = resolve_path(expression,data,current)) return *value;
    return nullptr;
}
struct Block { std::size_t body_end, close_end; std::size_t else_start = std::string_view::npos, else_end = std::string_view::npos; };
Block matching(std::string_view source, std::size_t start, std::string_view name) {
    std::vector<String> stack{String{name}};
    Block result{};
    while (start < source.size()) {
        auto open = source.find("{{",start);
        if (open == std::string_view::npos) break;
        const bool raw = source.substr(open).starts_with("{{{");
        auto close = source.find(raw ? "}}}" : "}}", open + (raw ? 3 : 2));
        if (close == std::string_view::npos) throw SyntaxError{"Unterminated view expression"};
        auto tag = trim(source.substr(open+2,close-open-2));
        start = close + (raw ? 3 : 2);
        if (raw || tag.starts_with('!')) continue;
        if (tag.starts_with('#')) { tag.remove_prefix(1); stack.emplace_back(tag.substr(0,tag.find(' '))); }
        else if (tag.starts_with('/')) {
            tag = trim(tag.substr(1));
            if (stack.empty() || stack.back() != tag) throw SyntaxError{"Mismatched view block: " + String{tag}};
            stack.pop_back();
            if (stack.empty()) { result.body_end = open; result.close_end = start; return result; }
        } else if (tag == "else" && stack.size() == 1) {
            if (result.else_start != std::string_view::npos) throw SyntaxError{"Duplicate else block"};
            result.else_start = open; result.else_end = start;
        }
    }
    throw SyntaxError{"Unclosed view block: " + String{name}};
}
String render_block(std::string_view source, const Data& data, const Value* current, RenderState& state, unsigned depth) {
    if (depth > 64) throw SyntaxError{"View composition nesting limit exceeded"};
    String output;
    auto append = [&](std::string_view text) {
        if (text.size() > state.remaining) throw SyntaxError{"View output limit exceeded"};
        state.remaining -= text.size(); output += text;
    };
    for (std::size_t cursor = 0; cursor < source.size();) {
        auto open = source.find("{{",cursor);
        if (open == std::string_view::npos) { append(source.substr(cursor)); break; }
        append(source.substr(cursor,open-cursor));
        bool raw = source.substr(open).starts_with("{{{");
        const auto width = raw ? 3U : 2U;
        auto close = source.find(raw ? "}}}" : "}}",open+width);
        if (close == std::string_view::npos) throw SyntaxError{"Unterminated view expression"};
        auto tag = trim(source.substr(open+width,close-open-width));
        cursor = close + width;
        if (tag.starts_with('!')) continue;
        if (tag.starts_with('/') || tag == "else") throw SyntaxError{"Unexpected view closing block"};
        if (tag.starts_with('#')) {
            tag.remove_prefix(1);
            auto space = tag.find(' ');
            auto name = tag.substr(0,space);
            auto expression = space == std::string_view::npos ? std::string_view{} : trim(tag.substr(space+1));
            auto block = matching(source,cursor,name);
            const auto body_end = block.else_start == std::string_view::npos ? block.body_end : block.else_start;
            auto body = source.substr(cursor,body_end-cursor);
            auto otherwise = block.else_end == std::string_view::npos ? std::string_view{} : source.substr(block.else_end,block.body_end-block.else_end);
            cursor = block.close_end;
            if (name == "each") {
                auto collection = evaluate(expression,data,current,state);
                if (collection.is_array() && !collection.as_array().empty()) {
                    const auto& items = collection.as_array();
                    for (std::size_t i = 0; i < items.size(); ++i) {
                        auto child = data;
                        child.with("loop", Value::object({{"index",Value{static_cast<UInt64>(i)}},{"first",Value{i==0}},{"last",Value{i+1==items.size()}},{"count",Value{static_cast<UInt64>(items.size())}}}));
                        append(render_block(body,child,&items[i],state,depth+1));
                    }
                } else append(render_block(otherwise,data,current,state,depth+1));
            } else if (name == "if" || name == "unless") {
                bool condition = evaluate(expression,data,current,state).truthy();
                if (name == "unless") condition = !condition;
                append(render_block(condition ? body : otherwise,data,current,state,depth+1));
            } else if (name == "section") {
                auto key = evaluate(expression,data,current,state).string();
                state.sections[key] = render_block(body,data,current,state,depth+1);
            } else if (name == "yield") {
                auto key = evaluate(expression,data,current,state).string();
                auto found = state.sections.find(key);
                append(found == state.sections.end() ? render_block(body,data,current,state,depth+1) : found->second);
            } else if (name == "layout" || name == "component") {
                auto params = arguments(expression,' ');
                if (params.empty()) throw SyntaxError{"View composition requires a template name"};
                auto child = data;
                for (std::size_t i=1;i<params.size();++i) {
                    auto equal = params[i].find('=');
                    if (equal == std::string_view::npos) throw SyntaxError{"View prop requires name=value"};
                    child.with(String{params[i].substr(0,equal)},evaluate(params[i].substr(equal+1),data,current,state));
                }
                auto saved_sections = state.sections;
                auto slot = render_block(body,child,current,state,depth+1);
                child.with("slot", slot); child.with("content", slot);
                append(render_block(state.engine.source(evaluate(params[0],data,current,state).string()),child,nullptr,state,depth+1));
                state.sections = std::move(saved_sections);
            } else throw SyntaxError{"Unknown view block: " + String{name}};
        } else if (tag.starts_with('>')) {
            auto params = arguments(trim(tag.substr(1)),' ');
            if (params.empty()) throw SyntaxError{"Partial requires a template name"};
            auto child = data;
            for (std::size_t i=1;i<params.size();++i) {
                auto equal = params[i].find('=');
                if (equal == std::string_view::npos) throw SyntaxError{"Partial prop requires name=value"};
                child.with(String{params[i].substr(0,equal)},evaluate(params[i].substr(equal+1),data,current,state));
            }
            append(render_block(state.engine.source(evaluate(params[0],data,current,state).string()),child,current,state,depth+1));
        } else {
            auto value = evaluate(tag,data,current,state).string();
            append(raw ? value : escape_html(value));
        }
    }
    return output;
}

} // namespace

Engine& Engine::helper(String name, Helper callback) {
    if (name.empty() || !callback) throw std::invalid_argument("View helper requires a name and callback");
    std::unique_lock lock{mutex_}; helpers_.insert_or_assign(std::move(name), std::move(callback)); return *this;
}
Value Engine::call(std::string_view name, const std::vector<Value>& arguments) const {
    Helper callback;
    { std::shared_lock lock{mutex_};
      const auto found = helpers_.find(String{name});
      if (found == helpers_.end()) throw SyntaxError{"Unknown view helper: " + String{name}};
      callback = found->second; }
    return callback(arguments);
}

Engine::Engine(
    std::filesystem::path root
)
    : root_(
        normalize_root(
            std::move(root)
        )
      ) {}

Engine& Engine::root(
    std::filesystem::path value
) {
    std::unique_lock lock{
        mutex_
    };

    root_ =
        normalize_root(
            std::move(value)
        );

    return *this;
}

std::filesystem::path
Engine::root() const {
    std::shared_lock lock{
        mutex_
    };

    return root_;
}

String Engine::render(std::string_view name, const Data& data) const {
    return render_text(source(name), data);
}
String Engine::source(std::string_view name) const {
    const auto path =
        resolve_template(
            root(),
            name
        );

    std::ifstream input{
        path,
        std::ios::binary
    };
    if (!input) {
        throw NotFound{path.string()};
    }

    const String source{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}
    };

    return source;
}

String Engine::render_text(
    std::string_view source,
    const Data& data
) const {
    RenderState state{*this};
    return render_block(source, data, nullptr, state, 0);
}

} // namespace gungnir::view
