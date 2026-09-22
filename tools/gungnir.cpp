#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <gungnir/cli/project.hpp>

namespace {

constexpr std::string_view version{
    "0.1.0"
};

void help() {
    std::cout
        << "Gungnir " << version << "\n\n"
        << "Usage:\n"
        << "  gungnir new <name> [path]\n"
        << "  gungnir build [--release]\n"
        << "  gungnir run [--release]\n"
        << "  gungnir dev\n"
        << "  gungnir make:model <name>\n"
        << "  gungnir make:controller <name>\n"
        << "  gungnir make:middleware <name>\n"
        << "  gungnir make:migration <name>\n"
        << "  gungnir make:request <name>\n"
        << "  gungnir make:job <name>\n"
        << "  gungnir migrate\n"
        << "  gungnir migrate:rollback\n"
        << "  gungnir migrate:reset\n"
        << "  gungnir migrate:status\n"
        << "  gungnir migrate:plan\n"
        << "  gungnir --version\n";
}

bool release_flag(
    const std::vector<std::string>& arguments
) {
    for (const auto& argument : arguments) {
        if (argument == "--release") {
            return true;
        }
    }

    return false;
}

} // namespace

int main(
    int argc,
    char** argv
) {
    if (argc < 2) {
        help();
        return 0;
    }

    const std::string command{
        argv[1]
    };

    if (
        command == "--help" ||
        command == "-h" ||
        command == "help"
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

    try {
        if (command == "new") {
            if (argc < 3) {
                throw std::invalid_argument(
                    "new requires a project name"
                );
            }

            const std::string name{
                argv[2]
            };

            const auto destination =
                argc >= 4
                    ? std::filesystem::path{
                        argv[3]
                    }
                    : std::filesystem::path{
                        name
                    };

            const auto project =
                gungnir::cli::Project::create(
                    destination,
                    name
                );

            std::cout
                << "Created Gungnir project: "
                << project.root().string()
                << '\n';

            return 0;
        }

        std::vector<std::string> arguments;

        for (
            int index = 2;
            index < argc;
            ++index
        ) {
            arguments.emplace_back(
                argv[index]
            );
        }

        auto project =
            gungnir::cli::Project::open();

        if (command == "build") {
            return project.build(
                release_flag(arguments)
            );
        }

        if (command == "dev") {
            return project.run(false);
        }

        if (command == "run") {
            return project.run(
                release_flag(arguments)
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
                release_flag(arguments)
            );
        }

        if (
            command == "make:model" ||
            command == "make:controller" ||
            command == "make:middleware" ||
            command == "make:migration" ||
            command == "make:request" ||
            command == "make:job"
        ) {
            if (argc < 3) {
                throw std::invalid_argument(
                    command +
                    " requires a name"
                );
            }

            std::filesystem::path created;

            if (command == "make:model") {
                created =
                    project.make_model(
                        argv[2]
                    );
            } else if (
                command ==
                "make:controller"
            ) {
                created =
                    project.make_controller(
                        argv[2]
                    );
            } else if (
                command ==
                "make:middleware"
            ) {
                created =
                    project.make_middleware(
                        argv[2]
                    );
            } else if (
                command ==
                "make:migration"
            ) {
                created =
                    project.make_migration(
                        argv[2]
                    );
            } else if (
                command ==
                "make:request"
            ) {
                created =
                    project.make_request(
                        argv[2]
                    );
            } else {
                created =
                    project.make_job(
                        argv[2]
                    );
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
