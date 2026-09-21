#include <gungnir/cli/project.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

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

String quote_shell(
    std::string_view argument
) {
#ifdef _WIN32
    String value{argument};
    String quoted{"\""};

    for (const char character : value) {
        if (character == '"') {
            quoted += "\\\"";
        } else {
            quoted += character;
        }
    }

    quoted += '"';
    return quoted;
#else
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
#endif
}

int execute(
    const std::vector<String>& arguments
) {
    if (arguments.empty()) {
        return 0;
    }

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
Project::assemble() const {
    const auto generated =
        root_ /
        ".gungnir" /
        "generated";

    std::filesystem::create_directories(
        generated
    );

    String output;

    output +=
        "#include <gungnir/gungnir.hpp>\n";
    output +=
        "#include <gungnir/orm/orm.hpp>\n\n";

    const std::vector<
        std::filesystem::path
    > source_directories{
        root_ / "app" / "models",
        root_ / "app" / "middleware",
        root_ / "app" / "controllers"
    };

    for (
        const auto& directory :
        source_directories
    ) {
        for (
            const auto& path :
            source_files(directory)
        ) {
            output += transpile_file(path);
            output += "\n\n";
        }
    }

    output +=
        "int main()\n"
        "{\n"
        "    auto app = "
        "gungnir::Application::create();\n\n";

    for (
        const auto& path :
        source_files(
            root_ /
            "routes"
        )
    ) {
        const auto routes =
            transpile_file(path);

        std::size_t start = 0;

        while (start < routes.size()) {
            const auto end =
                routes.find(
                    '\n',
                    start
                );

            output += "    ";
            output.append(
                routes,
                start,
                end ==
                    String::npos
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

    write_file(
        path,
        output
    );

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
        "target_compile_features(app PRIVATE cxx_std_23)\n"
        "target_link_libraries(\n"
        "    app\n"
        "    PRIVATE\n"
        "        gungnir::gungnir\n"
        "        gungnir::orm\n"
        ")\n"
    );

    return path;
}

int Project::build(
    bool release
) const {
    (void) assemble();

    const auto source =
        root_ /
        ".gungnir";

    const auto build =
        source /
        "build";

    std::vector<String> configure{
        "cmake",
        "-S",
        source.string(),
        "-B",
        build.string(),
        "-DCMAKE_BUILD_TYPE=" +
            String{
                release
                    ? "Release"
                    : "Debug"
            }
    };

    if (
        const char* prefix =
            std::getenv(
                "GUNGNIR_CMAKE_PREFIX"
            )
    ) {
        configure.push_back(
            "-DCMAKE_PREFIX_PATH=" +
            String{prefix}
        );
    }

    int code = execute(configure);

    if (code != 0) {
        return code;
    }

    return execute({
        "cmake",
        "--build",
        build.string(),
        "--config",
        release
            ? "Release"
            : "Debug"
    });
}

int Project::run(
    bool release
) const {
    const int built =
        build(release);

    if (built != 0) {
        return built;
    }

    auto executable =
        root_ /
        ".gungnir" /
        "build" /
        "app";

#ifdef _WIN32
    const auto configured =
        root_ /
        ".gungnir" /
        "build" /
        (
            release
                ? "Release"
                : "Debug"
        ) /
        "app.exe";

    if (
        std::filesystem::exists(
            configured
        )
    ) {
        executable = configured;
    } else {
        executable += ".exe";
    }
#endif

    const auto original =
        std::filesystem::current_path();

    std::filesystem::current_path(
        root_
    );

    const int code = execute({
        executable.string()
    });

    std::filesystem::current_path(
        original
    );

    return code;
}

} // namespace gungnir::cli
