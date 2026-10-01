#include <cstdlib>
#include <exception>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#include <gungnir/cli/lsp.hpp>
#include <gungnir/cli/project.hpp>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#include <stdlib.h>
#endif

#ifndef GUNGNIR_VERSION
#define GUNGNIR_VERSION "development"
#endif

namespace {

constexpr std::string_view version{
    GUNGNIR_VERSION
};

std::optional<std::filesystem::path>
existing_executable(
    std::filesystem::path candidate
) {
#ifdef _WIN32
    if (!candidate.has_extension()) {
        candidate += ".exe";
    }
#endif

    std::error_code error;
    if (
        !std::filesystem::is_regular_file(
            candidate,
            error
        )
    ) {
        return std::nullopt;
    }

    auto absolute =
        std::filesystem::absolute(
            candidate,
            error
        );

    if (error) {
        return candidate;
    }

    auto canonical =
        std::filesystem::weakly_canonical(
            absolute,
            error
        );

    return error
        ? std::optional<std::filesystem::path>{
            std::move(absolute)
        }
        : std::optional<std::filesystem::path>{
            std::move(canonical)
        };
}

std::optional<std::filesystem::path>
resolve_executable(
    std::string_view command
) {
    if (command.empty()) {
        return std::nullopt;
    }

    const std::filesystem::path requested{
        std::string{command}
    };

    if (
        requested.is_absolute() ||
        requested.has_parent_path()
    ) {
        return existing_executable(
            requested
        );
    }

    if (
        auto local =
            existing_executable(
                requested
            )
    ) {
        return local;
    }

    const char* configured_path =
        std::getenv("PATH");

    if (
        configured_path == nullptr ||
        *configured_path == '\0'
    ) {
        return std::nullopt;
    }

#ifdef _WIN32
    constexpr char separator = ';';
#else
    constexpr char separator = ':';
#endif

    const std::string path{
        configured_path
    };

    std::size_t start = 0;

    while (start <= path.size()) {
        const auto end =
            path.find(
                separator,
                start
            );

        auto entry =
            path.substr(
                start,
                end == std::string::npos
                    ? std::string::npos
                    : end - start
            );

        if (
            entry.size() >= 2 &&
            entry.front() == '"' &&
            entry.back() == '"'
        ) {
            entry =
                entry.substr(
                    1,
                    entry.size() - 2
                );
        }

        if (!entry.empty()) {
            if (
                auto resolved =
                    existing_executable(
                        std::filesystem::path{
                            entry
                        } /
                        requested
                    )
            ) {
                return resolved;
            }
        }

        if (end == std::string::npos) {
            break;
        }

        start = end + 1;
    }

    return std::nullopt;
}

bool is_installed_prefix(
    const std::filesystem::path& prefix
) {
    if (
        !std::filesystem::exists(
            prefix /
            "include/gungnir/gungnir.hpp"
        )
    ) {
        return false;
    }

    for (
        const auto* library_directory :
        {"lib", "lib64"}
    ) {
        if (
            std::filesystem::exists(
                prefix /
                library_directory /
                "cmake/Gungnir/GungnirConfig.cmake"
            )
        ) {
            return true;
        }
    }

    return false;
}

void configure_installed_prefix(
    std::string_view command
) {
    if (
        const char* configured =
            std::getenv(
                "GUNGNIR_CMAKE_PREFIX"
            );
        configured != nullptr &&
        *configured != '\0'
    ) {
        return;
    }

    const auto executable =
        resolve_executable(
            command
        );

    if (!executable) {
        return;
    }

    const auto prefix =
        executable
            ->parent_path()
            .parent_path();

    if (!is_installed_prefix(prefix)) {
        return;
    }

    const auto value =
        prefix.string();

#ifdef _WIN32
    (void) _putenv_s(
        "GUNGNIR_CMAKE_PREFIX",
        value.c_str()
    );
#else
    (void) ::setenv(
        "GUNGNIR_CMAKE_PREFIX",
        value.c_str(),
        0
    );
#endif
}

void help() {
    std::cout
        << "Gungnir "
        << version
        << "\n\n"
        << "  gungnir new <name> [path]\n"
        << "  gungnir build|run [--release]\n"
        << "  gungnir dev\n"
        << "  gungnir lsp\n"
        << "  gungnir make:model|controller|middleware|migration|request|job|event|mail <name>\n"
        << "  gungnir make:listener|policy|notification <name> <module.path::Type>\n"
        << "  gungnir migrate[:rollback|:reset|:status|:plan] [--release]\n";
}

} // namespace

int main(
    int argc,
    char** argv
) {
    try {
        configure_installed_prefix(
            argc > 0
                ? argv[0]
                : ""
        );

        const std::string command =
            argc > 1
                ? argv[1]
                : "help";

        if (
            command == "help" ||
            command == "--help" ||
            command == "-h"
        ) {
            help();
            return 0;
        }

        if (
            command == "--version" ||
            command == "-V"
        ) {
            std::cout
                << "Gungnir "
                << version
                << '\n';
            return 0;
        }

        if (command == "lsp") {
            if (argc != 2) {
                throw std::invalid_argument(
                    "Usage: gungnir lsp"
                );
            }

#ifdef _WIN32
            _setmode(
                _fileno(stdin),
                _O_BINARY
            );
            _setmode(
                _fileno(stdout),
                _O_BINARY
            );
#endif

            return gungnir::cli::
                run_language_server(
                    std::cin,
                    std::cout
                );
        }

        if (command == "new") {
            if (
                argc < 3 ||
                argc > 4
            ) {
                throw std::invalid_argument(
                    "Usage: gungnir new <name> [path]"
                );
            }

            const auto project =
                gungnir::cli::Project::create(
                    argc == 4
                        ? argv[3]
                        : argv[2],
                    argv[2]
                );

            std::cout
                << "Created Gungnir project: "
                << project.root().string()
                << '\n';

            return 0;
        }

        auto project =
            gungnir::cli::Project::open();

        if (command.starts_with("make:")) {
            if (
                argc < 3 ||
                argc > 4
            ) {
                throw std::invalid_argument(
                    "Generator requires a name and, where applicable, module.path::Type"
                );
            }

            const auto kind =
                command.substr(5);

            std::filesystem::path created;

            if (project.structured()) {
                created =
                    project.make(
                        kind,
                        argv[2],
                        argc == 4
                            ? argv[3]
                            : ""
                    );
            } else {
                if (argc != 3) {
                    throw std::invalid_argument(
                        "Legacy generators take one name"
                    );
                }

                if (kind == "model") {
                    created =
                        project.make_model(
                            argv[2]
                        );
                } else if (
                    kind == "controller"
                ) {
                    created =
                        project.make_controller(
                            argv[2]
                        );
                } else if (
                    kind == "middleware"
                ) {
                    created =
                        project.make_middleware(
                            argv[2]
                        );
                } else if (
                    kind == "migration"
                ) {
                    created =
                        project.make_migration(
                            argv[2]
                        );
                } else {
                    throw std::invalid_argument(
                        "Generator requires profile=structured"
                    );
                }
            }

            std::cout
                << "Created "
                << std::filesystem::relative(
                    created,
                    project.root()
                ).generic_string()
                << '\n';

            return 0;
        }

        if (command == "dev") {
            if (argc != 2) {
                throw std::invalid_argument(
                    "Usage: gungnir dev"
                );
            }

            return project.dev();
        }

        if (
            argc > 3 ||
            (
                argc == 3 &&
                std::string{argv[2]} !=
                    "--release"
            )
        ) {
            throw std::invalid_argument(
                "Only --release is supported for this command"
            );
        }

        const bool release =
            argc == 3;

        if (command == "build") {
            return project.build(
                release
            );
        }

        if (command == "run") {
            return project.run(
                release
            );
        }

        if (
            command == "migrate" ||
            command == "migrate:rollback" ||
            command == "migrate:reset" ||
            command == "migrate:status" ||
            command == "migrate:plan"
        ) {
            return project.migrate(
                command,
                release
            );
        }

        throw std::invalid_argument(
            "Unknown command: " +
            command
        );
    } catch (
        const std::exception& error
    ) {
        std::cerr
            << "gungnir: error: "
            << error.what()
            << '\n';

        return 1;
    }
}
