#include <gungnir/cli/project.hpp>
#include <gungnir/language/compiler.hpp>
#include <gungnir/language/semantic.hpp>
#include <gungnir/language/module.hpp>
#include <gungnir/language/transpiler.hpp>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>

namespace gungnir::cli {
namespace {
std::string read(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary);
    if (!in) throw std::runtime_error("Unable to read " + path.string());
    return {std::istreambuf_iterator<char>{in},{}};
}
void write(const std::filesystem::path& path,const std::string& content) {
    if (std::filesystem::is_regular_file(path) && read(path) == content) return;
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path,std::ios::binary | std::ios::trunc);
    out << content; out.close();
    if (!out) throw std::runtime_error("Unable to write " + path.string());
}
std::string quote(const std::string& value) { std::ostringstream out; out << std::quoted(value); return out.str(); }
std::vector<std::filesystem::path> files(const std::filesystem::path& root) {
    std::vector<std::filesystem::path> result;
    if (std::filesystem::is_directory(root)) for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
        if (entry.is_regular_file() && entry.path().extension() == ".gnr") result.push_back(entry.path());
    std::sort(result.begin(),result.end()); return result;
}
void check(const std::vector<language::Diagnostic>& diagnostics) {
    std::string message;
    for (const auto& d : diagnostics) if (d.level == language::DiagnosticLevel::error)
        message += d.location.file + ":" + std::to_string(d.location.line) + ":" + std::to_string(d.location.column) + " [" + d.code + "]: " + d.message + "\n";
    if (!message.empty()) throw std::runtime_error(message);
}
}
std::string bootstrap_template() {
    return R"cpp(#pragma once
#include <gungnir/core/services.hpp>
#include <gungnir/cache/memory_store.hpp>
#include <gungnir/queue/memory_driver.hpp>
#include <gungnir/mail/memory_transport.hpp>
#include <gungnir/storage/local_disk.hpp>

namespace bootstrap {
// Development adapters. Replace these explicitly for persistent production services.
inline void configure(gungnir::Application& app) {
    gungnir::ServiceOptions services;
    services.cache = std::make_shared<gungnir::cache::MemoryStore>();
    services.queue = std::make_shared<gungnir::queue::MemoryDriver>();
    services.mail = std::make_shared<gungnir::mail::MemoryTransport>();
    services.database_notifications = !app.config().string("database.default").empty();
    services.storage = std::make_shared<gungnir::storage::Manager>();
    services.storage->add("local", std::make_shared<gungnir::storage::LocalDisk>(app.base_path() / "storage"));
    app.provider<gungnir::ServicesProvider>(std::move(services));
    // Add service bindings, middleware, and providers here.
}
inline void boot(gungnir::Application&) {
    // Services and generated listeners, policies, and jobs are registered.
    // Add schedules here. Workers and schedulers are started explicitly by your app.
}
}
)cpp";
}

std::filesystem::path Project::make(std::string_view kind,String name,String dependency) {
    if (!structured()) throw std::logic_error("This generator requires profile=structured in .gungnir-project");
    auto type = normalize_class_name(name);
    const std::map<std::string,std::string> suffixes{{"controller","Controller"},{"middleware","Middleware"},{"job","Job"},{"listener","Listener"},{"policy","Policy"},{"notification","Notification"},{"mail","Mail"},{"request","Request"}};
    if (auto it = suffixes.find(std::string{kind}); it != suffixes.end() && !type.ends_with(it->second)) type += it->second;
    const std::map<std::string,std::string> directories{{"model","app/models"},{"controller","app/controllers"},{"middleware","app/middleware"},{"migration","database/migrations"},{"request","app/requests"},{"job","app/jobs"},{"event","app/events"},{"listener","app/listeners"},{"policy","app/policies"},{"notification","app/notifications"},{"mail","app/mail"}};
    auto directory = directories.find(std::string{kind});
    if (directory == directories.end()) throw std::invalid_argument("Unknown generator: " + std::string{kind});
    std::string prefix, related;
    if (kind == "listener" || kind == "policy" || kind == "notification") {
        auto separator = dependency.find("::");
        if (separator == std::string::npos) throw std::invalid_argument("Generator requires a dependency as module.path::Type");
        const auto module = dependency.substr(0,separator); related = dependency.substr(separator+2);
        if (module.empty() || related.empty() || related != normalize_class_name(related) ||
            module.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.") != std::string::npos ||
            module.starts_with('.') || module.ends_with('.') || module.find("..") != std::string::npos)
            throw std::invalid_argument("Invalid generator dependency");
        const auto path = language::ModuleResolver{root_}.resolve(module);
        const auto parsed = language::SyntaxParser{}.parse(read(path),path.generic_string(),module);
        check(parsed.diagnostics);
        const auto expected = kind == "listener" ? language::DeclarationKind::event : language::DeclarationKind::model;
        if (std::none_of(parsed.project.declarations.begin(),parsed.project.declarations.end(),[&](const auto& decl) { return decl.name == related && decl.kind == expected; }))
            throw std::invalid_argument("Dependency does not declare the required event/model: " + dependency);
        prefix = "import " + module + ";\n\n";
    } else if (!dependency.empty()) throw std::invalid_argument("This generator does not take a dependency");
    std::string body;
    if (kind == "model") body = "    string name;\n";
    if (kind == "controller") body = "    index() { return text(''); }\n";
    if (kind == "middleware") body = "    async Response handle(Request request, Next next) {\n        return await next(request);\n    }\n";
    if (kind == "migration") { const auto table = migration_table(name); body = "    up() {\n        Table::create('" + table + "', (table) => {\n            table.id();\n            table.timestamps();\n        });\n    }\n    down() { Table::dropIfExists('" + table + "'); }\n"; }
    if (kind == "job") body = "    handle() {}\n";
    if (kind == "event") body = "    int id;\n";
    if (kind == "listener") body = "    handle(" + related + " event) {}\n";
    if (kind == "policy") body = "    view(" + related + " actor, " + related + " resource) {\n        return actor.id == resource.id;\n    }\n";
    if (kind == "mail") body = "    subject() { return 'Welcome'; }\n    text() { return 'Welcome to Gungnir.'; }\n";
    if (kind == "notification") body = "    via(" + related + " recipient) { return ['database']; }\n    toDatabase(" + related + " recipient) { return { message: 'Welcome' }; }\n";
    auto content = prefix + std::string{kind} + " " + type + " {\n" + body + "}\n";
    if (kind == "request") content = "function Json validate" + type + "(Request request) {\n    return request.validate({ name: 'required|string' });\n}\n";
    // Parse before creating a file, so generator errors never leave broken sources.
    check(language::SyntaxParser{}.parse(content).diagnostics);
    return create_source(directory->second,snake_case(type) + ".gnr",content);
}

std::filesystem::path Project::assemble_structured() const {
    std::vector<std::filesystem::path> sources;
    for (auto it = std::filesystem::recursive_directory_iterator(root_); it != std::filesystem::recursive_directory_iterator{}; ++it) {
        const auto name = it->path().filename().string();
        if (it->is_directory() && (name == ".git" || name == ".gungnir" || name == "build" || name == "vendor" || name == "routes" || name == "storage")) { it.disable_recursion_pending(); continue; }
        if (it->is_regular_file() && it->path().extension() == ".gnr") sources.push_back(it->path());
    }
    const auto result = language::Compiler{}.compile_files(root_,sources);
    check(result.diagnostics);
    if (!result.validated) throw std::runtime_error("Project validation failed");
    const auto& project = *result.validated;
    const auto& syntax = project.syntax();
    const auto generated = root_ / ".gungnir/generated";
    const auto emitted = language::CppEmitter{}.emit_units(project);
    std::map<std::filesystem::path,std::string> outputs;
    outputs[generated / "program.hpp"] = emitted.declarations;
    std::string cmake = R"cmake(cmake_minimum_required(VERSION 3.25)
project(gungnir_app LANGUAGES CXX)
find_package(Gungnir CONFIG REQUIRED)
add_library(program STATIC generated/program.cpp
)cmake";
    outputs[generated / "program.cpp"] = "#include \"program.hpp\"\n";
    for (const auto& unit : emitted.units) {
        const auto filename = unit.module + ".cpp";
        outputs[generated / filename] = unit.code;
        cmake += "    generated/" + filename + "\n";
    }
    cmake += R"cmake()
target_compile_features(program PUBLIC cxx_std_23)
target_include_directories(program PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/generated" "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(program PUBLIC gungnir::gungnir gungnir::orm)
foreach(adapter postgresql mysql sqlserver)
    if(TARGET gungnir::${adapter})
        target_link_libraries(program PUBLIC gungnir::${adapter})
        string(TOUPPER "${adapter}" macro)
        target_compile_definitions(program PUBLIC "GNR_ADAPTER_${macro}=1")
    endif()
endforeach()
add_executable(app generated/app.cpp)
add_executable(migrations generated/migrations.cpp)
target_link_libraries(app PRIVATE program)
target_link_libraries(migrations PRIVATE program)
)cmake";
    outputs[root_ / ".gungnir/CMakeLists.txt"] = cmake;
    language::SemanticIndex index; index.closed_world = true;
    std::map<std::string,std::size_t> counts;
    for (const auto& decl : syntax.declarations) if (decl.kind != language::DeclarationKind::function) ++counts[decl.name];
    std::string aliases, registrations, factories, middleware;
    std::vector<std::pair<std::string,std::string>> migration_types;
    for (std::size_t d = 0; d < syntax.declarations.size(); ++d) {
        const auto& decl = syntax.declarations[d];
        const auto& resolved = project.declarations()[d];
        const auto& qualified = project.symbols()[resolved.symbol].cpp_name;
        using Kind = language::DeclarationKind;
        if (decl.kind == Kind::function) continue;
        if (counts[decl.name] == 1) {
            aliases += "using " + decl.name + " = " + qualified + ";\n";
            auto base = language::FrameworkBaseKind::event;
            if (decl.kind == Kind::controller) base = language::FrameworkBaseKind::controller;
            if (decl.kind == Kind::model) base = language::FrameworkBaseKind::model;
            if (decl.kind == Kind::middleware) base = language::FrameworkBaseKind::middleware;
            if (decl.kind == Kind::migration) base = language::FrameworkBaseKind::migration;
            index.types[decl.name] = base;
            index.declaration_sources[decl.name].insert(decl.origin.file);
            for (const auto& method : decl.methods) if (method.visibility == language::Visibility::public_) {
                index.actions[decl.name].insert(method.name);
                language::SemanticIndex::MethodSignature signature;
                for (const auto& parameter : method.parameters) signature.parameters.push_back(parameter.type.name);
                signature.return_type = method.result.name;
                index.methods[decl.name + "::" + method.name].push_back(std::move(signature));
            }
        }
        if (decl.kind == Kind::controller || decl.kind == Kind::middleware || decl.kind == Kind::listener || decl.kind == Kind::policy) {
            factories += "if (!app.container().has<" + qualified + ">()) app.bind<" + qualified + ">([](gungnir::Container& c) { return construct<" + qualified + ">(c); });\n";
        }
        if (decl.kind == Kind::middleware && counts[decl.name] == 1) middleware += "app.middleware_alias<" + qualified + ">(" + quote(decl.name) + ");\n";
        if (decl.kind == Kind::job) registrations += "gungnir::register_job<" + qualified + ">(app);\n";
        if (decl.kind == Kind::listener || decl.kind == Kind::policy)
            registrations += "gungnir::register_" + std::string{decl.kind == Kind::listener ? "listener" : "policy"} + "(app,app.container().resolve<" + qualified + ">());\n";
        if (decl.kind == Kind::migration) migration_types.emplace_back(std::filesystem::path{decl.origin.file}.stem().string(),qualified);
    }
    std::sort(migration_types.begin(),migration_types.end());
    std::string bootstrap = R"cpp(#pragma once
#include "program.hpp"
#include <gungnir/core/services.hpp>
#ifdef GNR_ADAPTER_POSTGRESQL
#include <gungnir/database/postgresql.hpp>
#endif
#ifdef GNR_ADAPTER_MYSQL
#include <gungnir/database/mysql.hpp>
#endif
#ifdef GNR_ADAPTER_SQLSERVER
#include <gungnir/database/sqlserver.hpp>
#endif
)cpp" + aliases;
    if (std::filesystem::exists(root_ / "bootstrap/app.hpp")) bootstrap += "#include <bootstrap/app.hpp>\n";
    else bootstrap += "namespace bootstrap { inline void configure(gungnir::Application& app) { app.provider<gungnir::ServicesProvider>(); } inline void boot(gungnir::Application&) {} }\n";
    bootstrap += R"cpp(namespace gungnir_generated {
template<class T> std::shared_ptr<T> construct(gungnir::Container& c) {
    if constexpr (requires { T::make(c); }) return T::make(c);
    else if constexpr (std::default_initializable<T>) return std::make_shared<T>();
    else throw std::logic_error("Register a factory in bootstrap::configure for declarations requiring constructor values");
}
inline void configure(gungnir::Application& app) {
#ifdef GNR_ADAPTER_POSTGRESQL
    gungnir::database::register_postgresql(app.database_drivers());
#endif
#ifdef GNR_ADAPTER_MYSQL
    gungnir::database::register_mysql(app.database_drivers());
#endif
#ifdef GNR_ADAPTER_SQLSERVER
    gungnir::database::register_sqlserver(app.database_drivers());
#endif
    bootstrap::configure(app);
)cpp" + factories + middleware + "app.on_boot([](gungnir::Application& app) {\n" + registrations + "bootstrap::boot(app);\n});\n}\n}\n";
    outputs[generated / "bootstrap.hpp"] = bootstrap;
    std::string app = "#include \"bootstrap.hpp\"\n#include <iostream>\nint main() { try {\nauto app = gungnir::Application::create();\ngungnir_generated::configure(app);\n";
    for (const auto& path : files(root_ / "routes")) {
        const auto routes = language::Transpiler{}.transpile(read(path),path.generic_string(),{.semantic_index = &index});
        check(routes.diagnostics); app += routes.code + "\n";
    }
    app += "app.run();\nreturn 0;\n} catch (const std::exception& e) { std::cerr << e.what() << '\\n'; return 1; } }\n";
    outputs[generated / "app.cpp"] = app;
    std::string migration = "#include \"bootstrap.hpp\"\n#include <gungnir/migration/runner.hpp>\n#include <iostream>\nint main(int argc,char** argv) { try {\nauto app = gungnir::Application::create();\ngungnir_generated::configure(app);\n";
    std::string entries;
    for (std::size_t i=0;i<migration_types.size();++i) {
        migration += migration_types[i].second + " migration_" + std::to_string(i) + ";\n";
        entries += "{" + quote(migration_types[i].first) + ",&migration_" + std::to_string(i) + "},\n";
    }
    migration += "std::vector<gungnir::migration::Named> migrations{\n" + entries + "};\n" + R"cpp(
const std::string command = argc > 1 ? argv[1] : "migrate";
if (command == "migrate:plan") {
    const auto configured = app.config().string("database.default");
    if (configured.empty()) throw std::logic_error("DB_CONNECTION is required for migration planning");
    const auto backend = gungnir::database::parse_backend(configured);
    for (const auto& item : migrations) {
        std::cout << "-- " << item.name << '\n';
        for (const auto& statement : gungnir::database::compile(item.migration->plan_up(),backend).statements) std::cout << statement.text << '\n';
    }
    return 0;
}
app.boot();
gungnir::migration::DatabaseRepository repository;
gungnir::migration::Runner runner{repository};
if (command == "migrate") std::cout << runner.migrate(migrations) << " migration(s) applied\n";
else if (command == "migrate:rollback") std::cout << runner.rollback(migrations) << " migration(s) rolled back\n";
else if (command == "migrate:reset") std::cout << runner.reset(migrations) << " migration(s) rolled back\n";
else if (command == "migrate:status") for (const auto& item : runner.status(migrations)) std::cout << (item.applied ? "[x] " : "[ ] ") << item.name << '\n';
else throw std::invalid_argument("Unknown migration command: " + command);
return 0;
} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; } }
)cpp";
    outputs[generated / "migrations.cpp"] = migration;
    // Validate the entire snapshot before changing any generated file. Unchanged
    // outputs retain timestamps across CLI invocations; obsolete units are no
    // longer listed in CMake, so deleted/renamed modules cannot stay linked.
    for (const auto& [path,content] : outputs) write(path,content);
    return generated / "app.cpp";
}
}
