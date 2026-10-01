#include <exception>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <gungnir/cli/project.hpp>
#include <gungnir/cli/lsp.hpp>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
namespace {
void help() {
    std::cout << "Gungnir 0.1.0\n\n"
        "  gungnir new <name> [path]\n"
        "  gungnir build|run [--release]\n"
        "  gungnir dev\n"
        "  gungnir lsp\n"
        "  gungnir make:model|controller|middleware|migration|request|job|event|mail <name>\n"
        "  gungnir make:listener|policy|notification <name> <module.path::Type>\n"
        "  gungnir migrate[:rollback|:reset|:status|:plan] [--release]\n";
}
}
int main(int argc,char** argv) {
    try {
        const std::string command=argc>1?argv[1]:"help";
        if(command=="help" || command=="--help" || command=="-h") { help(); return 0; }
        if(command=="--version" || command=="-V") { std::cout << "Gungnir 0.1.0\n"; return 0; }
        if(command=="lsp") {
            if(argc!=2) throw std::invalid_argument("Usage: gungnir lsp");
#ifdef _WIN32
            _setmode(_fileno(stdin),_O_BINARY); _setmode(_fileno(stdout),_O_BINARY);
#endif
            return gungnir::cli::run_language_server(std::cin,std::cout);
        }
        if(command=="new") {
            if(argc<3 || argc>4) throw std::invalid_argument("Usage: gungnir new <name> [path]");
            const auto project=gungnir::cli::Project::create(argc==4?argv[3]:argv[2],argv[2]);
            std::cout << "Created Gungnir project: " << project.root().string() << '\n'; return 0;
        }
        auto project=gungnir::cli::Project::open();
        if(command.starts_with("make:")) {
            if(argc<3 || argc>4) throw std::invalid_argument("Generator requires a name and, where applicable, module.path::Type");
            const auto kind=command.substr(5);
            std::filesystem::path created;
            if(project.structured()) created=project.make(kind,argv[2],argc==4?argv[3]:"");
            else {
                if(argc!=3) throw std::invalid_argument("Legacy generators take one name");
                if(kind=="model") created=project.make_model(argv[2]);
                else if(kind=="controller") created=project.make_controller(argv[2]);
                else if(kind=="middleware") created=project.make_middleware(argv[2]);
                else if(kind=="migration") created=project.make_migration(argv[2]);
                else throw std::invalid_argument("Generator requires profile=structured");
            }
            std::cout << "Created " << std::filesystem::relative(created,project.root()).generic_string() << '\n'; return 0;
        }
        if(command=="dev") { if(argc!=2) throw std::invalid_argument("Usage: gungnir dev"); return project.dev(); }
        if(argc>3 || (argc==3 && std::string{argv[2]}!="--release")) throw std::invalid_argument("Only --release is supported for this command");
        const bool release=argc==3;
        if(command=="build") return project.build(release);
        if(command=="run") return project.run(release);
        if(command=="migrate" || command=="migrate:rollback" || command=="migrate:reset" || command=="migrate:status" || command=="migrate:plan") return project.migrate(command,release);
        throw std::invalid_argument("Unknown command: " + command);
    } catch(const std::exception& e) { std::cerr << "gungnir: error: " << e.what() << '\n'; return 1; }
}
