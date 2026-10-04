#pragma once

#include <functional>
#include <memory>
#include <gungnir/http/uploads.hpp>
#include <gungnir/validation/rules.hpp>
#include <gungnir/validation/result.hpp>

namespace gungnir::validation {

class Engine;
struct Context {
    const std::vector<http::UploadedFile>* uploads{};
    const Engine* custom_rules{};
};

class Validator {
public:
    [[nodiscard]] static StructuredResult check(const http::Json& input, const Rules& rules);
    [[nodiscard]] static http::Json validate(const http::Json& input, const Rules& rules);
    [[nodiscard]] static StructuredResult check(const http::Json& input, const Rules& rules, Context context);
    [[nodiscard]] static http::Json validate(const http::Json& input, const Rules& rules, Context context);
    [[nodiscard]] static Result check(const Input& input, const Rules& rules);
    [[nodiscard]] static Input validate(
        const Input& input,
        const Rules& rules
    );
};

// An owning, explicitly passed rule registry. Copies share this registry;
// independently constructed engines remain isolated across applications.
class Engine {
public:
    using Predicate = std::function<bool(const http::Json&, const http::Json&)>;
    struct CustomRule { String message; Predicate predicate; };
    Engine();
    Engine(const std::shared_ptr<Engine>& registry);
    [[nodiscard]] static Engine make() { return Engine{}; }
    void extend(String name, String message, Predicate predicate) const;
    [[nodiscard]] CustomRule rule(std::string_view name) const;
    [[nodiscard]] StructuredResult check(const http::Json& input, const Rules& rules) const;
    [[nodiscard]] http::Json validate(const http::Json& input, const Rules& rules) const;
private:
    struct State;
    std::shared_ptr<State> state_;
};

class Report {
public:
    explicit Report(StructuredResult result) : result_(std::move(result)) {}
    [[nodiscard]] bool valid() const noexcept { return result_.valid(); }
    [[nodiscard]] bool failed() const noexcept { return !valid(); }
    [[nodiscard]] const http::Json& values() const noexcept { return result_.values; }
    [[nodiscard]] const Errors& errors() const noexcept { return result_.errors; }
private:
    StructuredResult result_;
};

} // namespace gungnir::validation
