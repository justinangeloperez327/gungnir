#include <gungnir/cli/project.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <process.h>
#endif

#include <gungnir/language/lexer.hpp>
#include <gungnir/language/transpiler.hpp>

namespace gungnir::cli {

namespace {

constexpr std::string_view project_marker{
    ".gungnir-project"
};

String read_file(
    const std::filesystem::path& path
) {
    std::ifstream input{
        path,
        std::ios::binary
    };

    if (!input) {
        throw std::runtime_error(
            "Unable to read file: " +
            path.string()
        );
    }

    return String{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}
    };
}

void write_file(
    const std::filesystem::path& path,
    std::string_view content
) {
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(
            path.parent_path()
        );
    }

    std::ofstream output{
        path,
        std::ios::binary |
            std::ios::trunc
    };

    if (!output) {
        throw std::runtime_error(
            "Unable to write file: " +
            path.string()
        );
    }

    output << content;
}

String trim(String value) {
    const auto first = std::find_if(
        value.begin(),
        value.end(),
        [](unsigned char character) {
            return std::isspace(character) == 0;
        }
    );

    const auto last = std::find_if(
        value.rbegin(),
        value.rend(),
        [](unsigned char character) {
            return std::isspace(character) == 0;
        }
    ).base();

    if (first >= last) {
        return {};
    }

    return String{first, last};
}

bool identifier_character(
    char character
) {
    const auto value =
        static_cast<unsigned char>(
            character
        );

    return
        std::isalnum(value) != 0 ||
        character == '_' ||
        character == '-' ||
        character == ' ';
}

#ifndef _WIN32
String quote_shell(
    std::string_view argument
) {
    const String value{argument};
    String quoted{"'"};

    for (const char character : value) {
        if (character == '\'') {
            quoted += "'\\''";
        } else {
            quoted += character;
        }
    }

    quoted += '\'';
    return quoted;
}
#endif

int execute(
    const std::vector<String>& arguments
) {
    if (arguments.empty()) {
        return 0;
    }

#ifdef _WIN32
    std::vector<const char*> argv;
    argv.reserve(arguments.size() + 1);

    for (const auto& argument : arguments) {
        argv.push_back(argument.c_str());
    }

    argv.push_back(nullptr);

    const auto result = _spawnvp(
        _P_WAIT,
        arguments.front().c_str(),
        argv.data()
    );

    return result == -1
        ? -1
        : static_cast<int>(result);
#else
    String command;

    for (
        std::size_t index = 0;
        index < arguments.size();
        ++index
    ) {
        if (index != 0) {
            command += ' ';
        }

        command += quote_shell(
            arguments[index]
        );
    }

    return std::system(
        command.c_str()
    );
#endif
}

std::vector<std::filesystem::path>
source_files(
    const std::filesystem::path& directory
) {
    std::vector<std::filesystem::path> files;

    if (!std::filesystem::exists(directory)) {
        return files;
    }

    for (
        const auto& entry :
        std::filesystem::recursive_directory_iterator{
            directory
        }
    ) {
        if (
            entry.is_regular_file() &&
            entry.path().extension() == ".gnr"
        ) {
            files.push_back(
                entry.path()
            );
        }
    }

    std::sort(
        files.begin(),
        files.end()
    );

    return files;
}

String transpile_file(
    const std::filesystem::path& path
) {
    language::Transpiler transpiler;

    const auto source =
        read_file(path);

    const auto result =
        transpiler.transpile(
            source,
            path.generic_string()
        );

    if (!result.success()) {
        String message{
            "Unable to transpile "
        };

        message += path.string();

        if (!result.diagnostics.empty()) {
            message += ": ";
            message +=
                result.diagnostics.front()
                    .message;
        }

        throw std::runtime_error(
            message
        );
    }

    return result.code;
}

String marker_name(
    const std::filesystem::path& root
) {
    const auto marker =
        root / project_marker;

    if (!std::filesystem::exists(marker)) {
        throw std::runtime_error(
            "Not a Gungnir project: " +
            root.string()
        );
    }

    auto name = trim(
        read_file(marker)
    );

    constexpr std::string_view prefix{
        "name="
    };

    if (name.starts_with(prefix)) {
        name.erase(
            0,
            prefix.size()
        );
    }

    name = trim(
        std::move(name)
    );

    if (name.empty()) {
        throw std::runtime_error(
            "Gungnir project marker is missing a name"
        );
    }

    return name;
}

} // namespace

Project Project::create(
    std::filesystem::path destination,
    String name
) {
    if (destination.empty()) {
        throw std::invalid_argument(
            "Project destination cannot be empty"
        );
    }

    name = trim(
        std::move(name)
    );

    if (name.empty()) {
        throw std::invalid_argument(
            "Project name cannot be empty"
        );
    }

    for (const char character : name) {
        if (!identifier_character(character)) {
            throw std::invalid_argument(
                "Project name contains an unsupported character"
            );
        }
    }

    destination =
        std::filesystem::absolute(
            std::move(destination)
        ).lexically_normal();

    if (
        std::filesystem::exists(destination) &&
        !std::filesystem::is_empty(
            destination
        )
    ) {
        throw std::runtime_error(
            "Project destination is not empty: " +
            destination.string()
        );
    }

    std::filesystem::create_directories(
        destination /
        "app" /
        "controllers"
    );

    std::filesystem::create_directories(
        destination /
        "app" /
        "models"
    );

    std::filesystem::create_directories(
        destination /
        "app" /
        "middleware"
    );

    std::filesystem::create_directories(
        destination /
        "database" /
        "migrations"
    );

    std::filesystem::create_directories(
        destination /
        "routes"
    );

    std::filesystem::create_directories(
        destination /
        "views"
    );

    std::filesystem::create_directories(
        destination /
        "config"
    );

    write_file(
        destination /
        project_marker,
        "name=" + name + "\n"
    );

    const String env =
        "APP_NAME=\"" + name + "\"\n"
        "APP_ENV=development\n"
        "APP_DEBUG=true\n"
        "APP_HOST=127.0.0.1\n"
        "APP_PORT=8000\n"
        "VIEW_PATH=views\n"
        "\n"
        "DB_CONNECTION=\n"
        "DB_NAME=default\n"
        "DB_POOL_SIZE=1\n"
        "DB_HOST=127.0.0.1\n"
        "DB_PORT=\n"
        "DB_DATABASE=\n"
        "DB_USERNAME=\n"
        "DB_PASSWORD=\n";

    write_file(
        destination / ".env",
        env
    );

    write_file(
        destination / ".env.example",
        env
    );

    write_file(
        destination / ".gitignore",
        ".gungnir/\n.env\nbuild/\n"
    );

    write_file(
        destination /
        "app" /
        "controllers" /
        "home_controller.gnr",
        "class HomeController : Controller\n"
        "{\n"
        "    Response index()\n"
        "    {\n"
        "        return view(\"welcome\", {\n"
        "            \"title\": \"Gungnir\"\n"
        "        });\n"
        "    }\n"
        "}\n"
    );

    write_file(
        destination /
        "routes" /
        "web.gnr",
        "Route::get(\"/\", HomeController::index);\n"
    );

    write_file(
        destination /
        "views" /
        "welcome.html",
        "<!doctype html>\n"
        "<html lang=\"en\">\n"
        "<head>\n"
        "    <meta charset=\"utf-8\">\n"
        "    <meta name=\"viewport\" "
        "content=\"width=device-width, initial-scale=1\">\n"
        "    <title>{{ title }}</title>\n"
        "</head>\n"
        "<body>\n"
        "    <main>\n"
        "        <h1>{{ title }}</h1>\n"
        "        <p>Your Gungnir application is running.</p>\n"
        "    </main>\n"
        "</body>\n"
        "</html>\n"
    );

    write_file(
        destination /
        "config" /
        ".gitkeep",
        ""
    );

    return Project{
        std::move(destination)
    };
}

Project Project::open(
    std::filesystem::path start
) {
    if (start.empty()) {
        start =
            std::filesystem::current_path();
    }

    auto current =
        std::filesystem::absolute(
            std::move(start)
        ).lexically_normal();

    if (
        std::filesystem::is_regular_file(
            current
        )
    ) {
        current =
            current.parent_path();
    }

    while (!current.empty()) {
        if (
            std::filesystem::exists(
                current /
                project_marker
            )
        ) {
            return Project{
                current
            };
        }

        const auto parent =
            current.parent_path();

        if (
            parent.empty() ||
            parent == current
        ) {
            break;
        }

        current = parent;
    }

    throw std::runtime_error(
        "No Gungnir project found from the current path"
    );
}

Project::Project(
    std::filesystem::path root
)
    : root_(
        std::filesystem::absolute(
            std::move(root)
        ).lexically_normal()
    ) {
    (void) marker_name(root_);
}

const std::filesystem::path&
Project::root() const noexcept {
    return root_;
}

String Project::name() const {
    return marker_name(root_);
}

String Project::normalize_class_name(
    std::string_view value
) {
    if (value.empty()) {
        throw std::invalid_argument(
            "Generated class name cannot be empty"
        );
    }

    String result;
    bool uppercase = true;

    for (const char character : value) {
        const auto byte =
            static_cast<unsigned char>(
                character
            );

        if (
            character == '_' ||
            character == '-' ||
            character == ' '
        ) {
            uppercase = true;
            continue;
        }

        if (
            std::isalnum(byte) == 0
        ) {
            throw std::invalid_argument(
                "Generated class name contains an unsupported character"
            );
        }

        if (result.empty()) {
            if (
                std::isalpha(byte) == 0 &&
                character != '_'
            ) {
                throw std::invalid_argument(
                    "Generated class name must begin with a letter"
                );
            }
        }

        if (uppercase) {
            result += static_cast<char>(
                std::toupper(byte)
            );
            uppercase = false;
        } else {
            result += character;
        }
    }

    if (result.empty()) {
        throw std::invalid_argument(
            "Generated class name cannot be empty"
        );
    }

    return result;
}

String Project::snake_case(
    std::string_view value
) {
    String result;
    result.reserve(value.size() + 8);

    char previous = '\0';

    for (const char character : value) {
        const auto byte =
            static_cast<unsigned char>(
                character
            );

        if (
            character == '-' ||
            character == ' '
        ) {
            if (
                !result.empty() &&
                result.back() != '_'
            ) {
                result += '_';
            }

            previous = character;
            continue;
        }

        if (
            std::isupper(byte) != 0
        ) {
            if (
                !result.empty() &&
                result.back() != '_' &&
                (
                    std::islower(
                        static_cast<unsigned char>(
                            previous
                        )
                    ) != 0 ||
                    std::isdigit(
                        static_cast<unsigned char>(
                            previous
                        )
                    ) != 0
                )
            ) {
                result += '_';
            }

            result += static_cast<char>(
                std::tolower(byte)
            );
        } else {
            result += character;
        }

        previous = character;
    }

    return result;
}

String Project::migration_table(
    std::string_view value
) {
    String normalized = snake_case(value);

    constexpr std::string_view create_prefix{
        "create_"
    };

    constexpr std::string_view table_suffix{
        "_table"
    };

    if (
        normalized.starts_with(
            create_prefix
        )
    ) {
        normalized.erase(
            0,
            create_prefix.size()
        );
    }

    if (
        normalized.ends_with(
            table_suffix
        )
    ) {
        normalized.erase(
            normalized.size() -
                table_suffix.size()
        );
    }

    return normalized.empty()
        ? "table_name"
        : normalized;
}

std::filesystem::path
Project::create_source(
    const std::filesystem::path& directory,
    const String& file_name,
    const String& content
) const {
    const auto path =
        root_ /
        directory /
        file_name;

    if (std::filesystem::exists(path)) {
        throw std::runtime_error(
            "Generated file already exists: " +
            path.string()
        );
    }

    write_file(
        path,
        content
    );

    return path;
}

std::filesystem::path
Project::make_model(
    String name
) {
    const auto class_name =
        normalize_class_name(name);

    return create_source(
        "app/models",
        snake_case(class_name) + ".gnr",
        "class " + class_name +
        " : Model\n"
        "{\n"
        "    string name;\n"
        "}\n"
    );
}

std::filesystem::path
Project::make_controller(
    String name
) {
    auto class_name =
        normalize_class_name(name);

    constexpr std::string_view suffix{
        "Controller"
    };

    if (!class_name.ends_with(suffix)) {
        class_name += suffix;
    }

    return create_source(
        "app/controllers",
        snake_case(class_name) +
            ".gnr",
        "class " + class_name +
        " : Controller\n"
        "{\n"
        "    Response index()\n"
        "    {\n"
        "        return response();\n"
        "    }\n"
        "}\n"
    );
}

std::filesystem::path
Project::make_middleware(
    String name
) {
    auto class_name =
        normalize_class_name(name);

    constexpr std::string_view suffix{
        "Middleware"
    };

    if (!class_name.ends_with(suffix)) {
        class_name += suffix;
    }

    return create_source(
        "app/middleware",
        snake_case(class_name) +
            ".gnr",
        "class " + class_name +
        " : Middleware\n"
        "{\n"
        "    async Response handle("
        "Request request, Next next)\n"
        "    {\n"
        "        return await next(request);\n"
        "    }\n"
        "}\n"
    );
}

std::filesystem::path
Project::make_migration(
    String name
) {
    const auto class_name =
        normalize_class_name(name);

    const auto table =
        migration_table(name);

    return create_source(
        "database/migrations",
        snake_case(class_name) +
            ".gnr",
        "class " + class_name +
        " : Migration\n"
        "{\n"
        "    void up()\n"
        "    {\n"
        "        Table::create(\"" +
            table +
            "\", [](Column& column) {\n"
        "            column.id();\n"
        "            column.timestamps();\n"
        "        });\n"
        "    }\n"
        "\n"
        "    void down()\n"
        "    {\n"
        "        Table::drop_if_exists(\"" +
            table +
            "\");\n"
        "    }\n"
        "}\n"
    );
}


std::filesystem::path
Project::make_request(
    String
) {
    throw std::logic_error(
        "make:request is not available until ValidatedRequest source-language lowering is implemented"
    );
}

std::filesystem::path
Project::make_job(
    String
) {
    throw std::logic_error(
        "make:job is not available until queue Job source-language lowering is implemented"
    );
}

namespace {

std::optional<std::size_t> next_significant(
    const std::vector<language::Token>& tokens,
    std::size_t index
) {
    for (auto cursor = index + 1; cursor < tokens.size(); ++cursor) {
        if (!tokens[cursor].trivia() && tokens[cursor].kind != language::TokenKind::end) {
            return cursor;
        }
    }

    return std::nullopt;
}

String migration_class_name(
    const std::filesystem::path& path
) {
    const auto source = read_file(path);
    language::Lexer lexer{source};
    const auto tokens = lexer.tokenize();

    for (std::size_t index = 0; index < tokens.size(); ++index) {
        if (tokens[index].trivia() || tokens[index].lexeme != "class") {
            continue;
        }

        const auto name = next_significant(tokens, index);
        const auto colon = name ? next_significant(tokens, *name) : std::nullopt;
        const auto base = colon ? next_significant(tokens, *colon) : std::nullopt;

        if (
            name &&
            colon &&
            base &&
            tokens[*name].kind == language::TokenKind::identifier &&
            tokens[*colon].lexeme == ":" &&
            tokens[*base].lexeme == "Migration"
        ) {
            return tokens[*name].lexeme;
        }
    }

    throw std::runtime_error(
        "Migration file does not declare a Migration class: " +
        path.string()
    );
}

std::vector<String> configure_arguments(
    const std::filesystem::path& root,
    bool release
) {
    const auto source = root / ".gungnir";
    const auto build = source / "build";

    std::vector<String> arguments{
        "cmake",
        "-S",
        source.string(),
        "-B",
        build.string(),
        "-DCMAKE_BUILD_TYPE=" +
            String{release ? "Release" : "Debug"}
    };

    if (const char* prefix = std::getenv("GUNGNIR_CMAKE_PREFIX")) {
        arguments.push_back(
            "-DCMAKE_PREFIX_PATH=" +
            String{prefix}
        );
    }

    return arguments;
}

int configure_generated(
    const std::filesystem::path& root,
    bool release
) {
    return execute(
        configure_arguments(
            root,
            release
        )
    );
}

int build_generated(
    const std::filesystem::path& root,
    bool release,
    std::string_view target = {}
) {
    std::vector<String> arguments{
        "cmake",
        "--build",
        (root / ".gungnir" / "build").string(),
        "--config",
        release ? "Release" : "Debug"
    };

    if (!target.empty()) {
        arguments.push_back("--target");
        arguments.push_back(String{target});
    }

    return execute(arguments);
}

std::filesystem::path generated_executable(
    const std::filesystem::path& root,
    std::string_view name,
    bool release
) {
    auto executable =
        root /
        ".gungnir" /
        "build" /
        String{name};

#ifdef _WIN32
    const auto configured =
        root /
        ".gungnir" /
        "build" /
        (release ? "Release" : "Debug") /
        (String{name} + ".exe");

    if (std::filesystem::exists(configured)) {
        executable = configured;
    } else {
        executable += ".exe";
    }
#else
    (void) release;
#endif

    return executable;
}

} // namespace

std::filesystem::path
Project::assemble_migrations() const {
    const auto generated =
        root_ /
        ".gungnir" /
        "generated";

    std::filesystem::create_directories(
        generated
    );

    const auto files =
        source_files(
            root_ /
            "database" /
            "migrations"
        );

    String output;

    output += "#include <gungnir/gungnir.hpp>\n";
    output += "#include <gungnir/database/database.hpp>\n";
    output += "#include <gungnir/migration/runner.hpp>\n";
    output += "#include <iostream>\n";
    output += "#include <stdexcept>\n";
    output += "#include <string>\n";
    output += "#include <vector>\n\n";

    std::vector<String> classes;
    classes.reserve(files.size());

    for (const auto& path : files) {
        classes.push_back(
            migration_class_name(path)
        );

        output += transpile_file(path);
        output += "\n\n";
    }

    output +=
        "int main(int argc, char** argv)\n"
        "{\n"
        "    try {\n"
        "        auto app = gungnir::Application::create();\n\n";

    for (std::size_t index = 0; index < files.size(); ++index) {
        output +=
            "        " +
            classes[index] +
            " migration_" +
            std::to_string(index) +
            ";\n";
    }

    output +=
        "\n        const std::vector<gungnir::migration::Named> migrations{\n";

    for (std::size_t index = 0; index < files.size(); ++index) {
        output +=
            "            {\"" +
            files[index].stem().string() +
            "\", &migration_" +
            std::to_string(index) +
            "}";

        if (index + 1 < files.size()) {
            output += ",";
        }

        output += "\n";
    }

    output +=
        "        };\n\n"
        "        const std::string command = argc > 1 ? argv[1] : \"migrate\";\n\n"
        "        if (command == \"migrate:plan\") {\n"
        "            const auto configured = app.config().string(\"database.default\");\n"
        "            if (configured.empty()) {\n"
        "                throw std::logic_error(\"DB_CONNECTION is required for migration planning\");\n"
        "            }\n"
        "            const auto backend = gungnir::database::parse_backend(configured);\n"
        "            for (const auto& item : migrations) {\n"
        "                std::cout << \"-- \" << item.name << '\\n';\n"
        "                const auto compiled = gungnir::database::compile(item.migration->plan_up(), backend);\n"
        "                for (const auto& statement : compiled.statements) {\n"
        "                    std::cout << statement.text << '\\n';\n"
        "                }\n"
        "            }\n"
        "            return 0;\n"
        "        }\n\n"
        "        app.boot();\n"
        "        gungnir::migration::DatabaseRepository repository;\n"
        "        gungnir::migration::Runner runner{repository};\n\n"
        "        if (command == \"migrate\") {\n"
        "            std::cout << runner.migrate(migrations) << \" migration(s) applied\\n\";\n"
        "            return 0;\n"
        "        }\n"
        "        if (command == \"migrate:rollback\") {\n"
        "            std::cout << runner.rollback(migrations) << \" migration(s) rolled back\\n\";\n"
        "            return 0;\n"
        "        }\n"
        "        if (command == \"migrate:reset\") {\n"
        "            std::cout << runner.reset(migrations) << \" migration(s) rolled back\\n\";\n"
        "            return 0;\n"
        "        }\n"
        "        if (command == \"migrate:status\") {\n"
        "            for (const auto& status : runner.status(migrations)) {\n"
        "                std::cout << (status.applied ? \"[x] \" : \"[ ] \") << status.name;\n"
        "                if (status.applied) {\n"
        "                    std::cout << \"  batch \" << status.batch;\n"
        "                }\n"
        "                std::cout << '\\n';\n"
        "            }\n"
        "            return 0;\n"
        "        }\n\n"
        "        throw std::invalid_argument(\"Unknown migration command: \" + command);\n"
        "    } catch (const std::exception& error) {\n"
        "        std::cerr << \"gungnir migrations: error: \" << error.what() << '\\n';\n"
        "        return 1;\n"
        "    }\n"
        "}\n";

    const auto path =
        generated /
        "migrations.cpp";

    write_file(path, output);
    return path;
}

std::filesystem::path
Project::assemble() const {
    const auto generated =
        root_ /
        ".gungnir" /
        "generated";

    std::filesystem::create_directories(
        generated
    );

    String output;

    output += "#include <gungnir/gungnir.hpp>\n";
    output += "#include <gungnir/orm/orm.hpp>\n\n";

    const std::vector<std::filesystem::path> source_directories{
        root_ / "app" / "models",
        root_ / "app" / "middleware",
        root_ / "app" / "controllers"
    };

    for (const auto& directory : source_directories) {
        for (const auto& path : source_files(directory)) {
            output += transpile_file(path);
            output += "\n\n";
        }
    }

    output +=
        "int main()\n"
        "{\n"
        "    auto app = gungnir::Application::create();\n\n";

    for (const auto& path : source_files(root_ / "routes")) {
        const auto routes = transpile_file(path);
        std::size_t start = 0;

        while (start < routes.size()) {
            const auto end = routes.find('\n', start);
            output += "    ";

            output.append(
                routes,
                start,
                end == String::npos
                    ? String::npos
                    : end - start
            );

            output += '\n';

            if (end == String::npos) {
                break;
            }

            start = end + 1;
        }

        output += '\n';
    }

    output +=
        "    app.run();\n"
        "    return 0;\n"
        "}\n";

    const auto path =
        generated /
        "app.cpp";

    write_file(path, output);
    (void) assemble_migrations();

    write_file(
        root_ /
        ".gungnir" /
        "CMakeLists.txt",
        "cmake_minimum_required(VERSION 3.25)\n"
        "project(gungnir_app LANGUAGES CXX)\n"
        "\n"
        "find_package(Gungnir CONFIG REQUIRED)\n"
        "\n"
        "add_executable(app generated/app.cpp)\n"
        "add_executable(migrations generated/migrations.cpp)\n"
        "\n"
        "target_compile_features(app PRIVATE cxx_std_23)\n"
        "target_compile_features(migrations PRIVATE cxx_std_23)\n"
        "\n"
        "target_link_libraries(app PRIVATE gungnir::gungnir gungnir::orm)\n"
        "target_link_libraries(migrations PRIVATE gungnir::gungnir gungnir::orm)\n"
    );

    return path;
}

int Project::build(
    bool release
) const {
    (void) assemble();

    const int configured =
        configure_generated(root_, release);

    if (configured != 0) {
        return configured;
    }

    return build_generated(
        root_,
        release
    );
}

int Project::run(
    bool release
) const {
    const int built =
        build(release);

    if (built != 0) {
        return built;
    }

    const auto executable =
        generated_executable(
            root_,
            "app",
            release
        );

    const auto original =
        std::filesystem::current_path();

    std::filesystem::current_path(root_);

    const int code =
        execute({
            executable.string()
        });

    std::filesystem::current_path(original);
    return code;
}

int Project::migrate(
    String command,
    bool release
) const {
    (void) assemble();

    const int configured =
        configure_generated(root_, release);

    if (configured != 0) {
        return configured;
    }

    const int built =
        build_generated(
            root_,
            release,
            "migrations"
        );

    if (built != 0) {
        return built;
    }

    const auto executable =
        generated_executable(
            root_,
            "migrations",
            release
        );

    const auto original =
        std::filesystem::current_path();

    std::filesystem::current_path(root_);

    const int code =
        execute({
            executable.string(),
            command
        });

    std::filesystem::current_path(original);
    return code;
}

} // namespace gungnir::cli
