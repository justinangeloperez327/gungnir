#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <gungnir/language/diagnostic.hpp>

namespace gungnir::language {
using SyntaxId = std::size_t;
using TypeId = std::size_t;
using SymbolId = std::size_t;
inline constexpr std::size_t invalid_id = static_cast<std::size_t>(-1);

struct Origin {
    std::string file;
    std::size_t line{1}, column{1};
    // Half-open byte offsets in the original source file.
    std::size_t begin{0}, end{0};
};
struct TypeSyntax { std::string name; std::vector<TypeSyntax> arguments; bool optional{false}; Origin origin; };
struct ParameterSyntax { std::string name; TypeSyntax type; SyntaxId default_value{invalid_id}; Origin origin; };
enum class SyntaxExpressionKind { literal, name, member, call, unary, binary, subscript, list, object, lambda, await_, conditional };
enum class SyntaxStatementKind { binding, expression, return_, block, if_, while_, for_, for_in, break_, continue_, throw_ };
enum class DeclarationKind { function, model, controller, migration, middleware, policy, event, listener, notification, mail, job };
enum class Visibility { public_, protected_, private_ };
struct SyntaxExpression {
    SyntaxExpressionKind kind{SyntaxExpressionKind::literal};
    Origin origin;
    std::string text;
    std::string literal_type;
    std::vector<SyntaxId> operands;
    std::vector<std::string> argument_names;
    std::vector<ParameterSyntax> parameters;
    std::vector<SyntaxId> body;
};
struct SyntaxStatement {
    SyntaxStatementKind kind{SyntaxStatementKind::expression};
    Origin origin;
    std::string name;
    bool immutable{true};
    std::optional<TypeSyntax> declared_type;
    SyntaxId expression{invalid_id};
    std::vector<SyntaxId> body, alternative;
    std::vector<SyntaxId> parts;
};
struct CallableSyntax {
    Origin origin;
    std::string name;
    TypeSyntax result;
    bool implicit_result{false}, asynchronous{false};
    Visibility visibility{Visibility::public_};
    std::vector<ParameterSyntax> parameters;
    std::vector<SyntaxId> body;
};
struct FieldSyntax { Origin origin; std::string name; TypeSyntax type; bool injection{false}; SyntaxId initializer{invalid_id}; Visibility visibility{Visibility::public_}; bool inferred_type{false}; };
struct MetadataSyntax { Origin origin; std::string name; SyntaxId value{invalid_id}; };
struct RelationshipSyntax {
    Origin origin;
    std::string name, kind;
    std::vector<TypeSyntax> types;
    std::vector<SyntaxId> arguments;
    std::vector<std::string> argument_names;
};
struct DeclarationSyntax {
    Origin origin;
    DeclarationKind kind{DeclarationKind::function};
    std::string name;
    std::size_t module{0};
    std::vector<FieldSyntax> fields;
    std::vector<MetadataSyntax> metadata;
    std::vector<CallableSyntax> methods;
    std::vector<RelationshipSyntax> relationships;
};
struct ImportSyntax { Origin origin; std::string module, alias; };
struct RouteModifierSyntax {
    Origin origin;
    std::string name;
    std::vector<SyntaxId> arguments;
    std::optional<TypeSyntax> middleware_type;
};
struct RouteSyntax {
    Origin origin;
    std::size_t module{0};
    std::string method;
    std::vector<SyntaxId> arguments;
    std::vector<RouteModifierSyntax> modifiers;
    bool group{false};
    std::vector<RouteSyntax> children;
};
struct ModuleSyntax { Origin origin; std::string name; std::vector<ImportSyntax> imports; std::vector<SyntaxId> declarations; std::vector<SyntaxId> routes; };
struct SyntaxProject {
    std::vector<ModuleSyntax> modules;
    std::vector<DeclarationSyntax> declarations;
    std::vector<SyntaxExpression> expressions;
    std::vector<SyntaxStatement> statements;
    std::vector<RouteSyntax> routes;
};
struct SyntaxResult { SyntaxProject project; std::vector<Diagnostic> diagnostics; };
class SyntaxParser {
public:
    [[nodiscard]] SyntaxResult parse(std::string_view source, std::string file = "<memory>", std::string module = {}) const;
};
struct NativeParameter { std::string name, type; bool optional{false}; };
struct NativeCallable {
    std::string owner, name, result, cpp_name;
    std::vector<NativeParameter> parameters;
    bool asynchronous{false};
};
struct NativeField { std::string owner, name, type; };
struct NativeType { std::string name, cpp_name; };
struct CompilerOptions {
    bool emit_line_directives{true};
    std::vector<NativeType> native_types;
    std::vector<NativeCallable> native_callables;
    std::vector<NativeField> native_fields;
    // Validation-only mode stops at ValidatedProject. It is used by
    // authoritative semantic checks and compiler inspection tooling.
    bool validate_only{false};
};
struct ResolvedType { std::string name, cpp_name; std::vector<TypeId> arguments; bool optional{false}; };
enum class ResolvedSymbolKind { declaration, callable, parameter, local, field, injection, builtin };
struct ResolvedSymbol {
    ResolvedSymbolKind kind{ResolvedSymbolKind::local};
    std::string name, cpp_name;
    TypeId type{invalid_id};
    SymbolId owner{invalid_id};
    bool immutable{false}, asynchronous{false};
    Visibility visibility{Visibility::public_};
    std::vector<TypeId> parameters;
    std::vector<std::string> parameter_names;
    std::vector<SyntaxId> defaults;
    bool receives_receiver{false};
};
struct ExpressionResolution {
    TypeId type{invalid_id};
    SymbolId symbol{invalid_id};
    // Bound argument order and casts are fixed before C++ emission.
    std::vector<std::size_t> argument_order;
    std::vector<TypeId> argument_conversions;
    std::vector<SymbolId> captures;
    std::vector<SymbolId> parameters;
    // Free framework helpers may receive the enclosing model rather than
    // the relationship field that appeared in the source call.
    SyntaxId receiver_expression{invalid_id};
};
struct CallableResolution { SymbolId symbol{invalid_id}; std::vector<SymbolId> parameters; bool all_paths_return{false}; };
struct RelationshipResolution {
    SymbolId field{invalid_id};
    TypeId related{invalid_id}, through{invalid_id};
    std::vector<std::string> keys;
};
struct DeclarationResolution { SymbolId symbol{invalid_id}; std::vector<SymbolId> fields; std::vector<CallableResolution> methods; std::vector<RelationshipResolution> relationships; };
struct RouteMiddlewareResolution { SymbolId type{invalid_id}; std::string alias; };
struct RouteConstraintResolution { std::string parameter, expression; };
struct RouteResolution {
    Origin origin;
    std::size_t module{0};
    std::string method, path, name;
    SymbolId controller{invalid_id}, action{invalid_id};
    std::vector<std::string> parameters;
    std::vector<RouteMiddlewareResolution> middleware;
    std::vector<RouteConstraintResolution> constraints;
};
class ValidatedProject {
public:
    ValidatedProject(const ValidatedProject&) = default;
    ValidatedProject(ValidatedProject&&) noexcept = default;
    ValidatedProject& operator=(const ValidatedProject&) = default;
    ValidatedProject& operator=(ValidatedProject&&) noexcept = default;
    [[nodiscard]] const SyntaxProject& syntax() const noexcept { return syntax_; }
    [[nodiscard]] const std::vector<ResolvedType>& types() const noexcept { return types_; }
    [[nodiscard]] const std::vector<ResolvedSymbol>& symbols() const noexcept { return symbols_; }
    [[nodiscard]] const std::vector<ExpressionResolution>& expressions() const noexcept { return expressions_; }
    [[nodiscard]] const std::vector<SymbolId>& bindings() const noexcept { return bindings_; }
    [[nodiscard]] const std::vector<DeclarationResolution>& declarations() const noexcept { return declarations_; }
    [[nodiscard]] const std::vector<std::size_t>& declaration_order() const noexcept { return declaration_order_; }
    [[nodiscard]] const std::vector<std::size_t>& module_order() const noexcept { return module_order_; }
    [[nodiscard]] const std::vector<RouteResolution>& routes() const noexcept { return routes_; }
private:
    ValidatedProject() = default;
    friend class ProgramValidator;
    friend class ValidationEngine;
    SyntaxProject syntax_;
    std::vector<ResolvedType> types_;
    std::vector<ResolvedSymbol> symbols_;
    std::vector<ExpressionResolution> expressions_;
    std::vector<SymbolId> bindings_;
    std::vector<DeclarationResolution> declarations_;
    std::vector<RouteResolution> routes_;
    std::vector<std::size_t> module_order_, declaration_order_;
};
struct ValidationResult { std::optional<ValidatedProject> project; std::vector<Diagnostic> diagnostics; };
class ProgramValidator {
public:
    [[nodiscard]] ValidationResult validate(SyntaxProject project, const CompilerOptions& options = {}) const;
};
struct CppIrProject;
struct EmittedUnit { std::string module, code; };
struct EmittedProject { std::string declarations; std::vector<EmittedUnit> units; };
class CppEmitter {
public:
    // Canonical backend API: serialize already-lowered C++ IR.
    [[nodiscard]] std::string emit(const CppIrProject& project) const;
    [[nodiscard]] EmittedProject emit_units(const CppIrProject& project) const;

    // Pre-1.0 convenience wrappers. These lower through CppIrLowerer first.
    [[nodiscard]] std::string emit(const ValidatedProject& project, bool line_directives = true) const;
    [[nodiscard]] EmittedProject emit_units(const ValidatedProject& project, bool line_directives = true) const;
};
struct CompilationResult {
    std::string code;
    std::vector<Diagnostic> diagnostics;
    std::optional<ValidatedProject> validated;
    [[nodiscard]] bool success() const noexcept;
};
struct SourceFile { std::string file, module, source; };
class Compiler {
public:
    [[nodiscard]] CompilationResult compile_sources(std::vector<SourceFile> files, const CompilerOptions& options = {}) const;
    [[nodiscard]] CompilationResult compile(std::string_view source, std::string file = "<memory>", const CompilerOptions& options = {}) const;
    [[nodiscard]] CompilationResult compile_project(const std::filesystem::path& root, const CompilerOptions& options = {}) const;
    [[nodiscard]] CompilationResult compile_files(const std::filesystem::path& root, std::vector<std::filesystem::path> files, const CompilerOptions& options = {}) const;
};
[[nodiscard]] std::string dump_validated(const ValidatedProject& project);
} // namespace gungnir::language
