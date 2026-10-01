#include <gungnir/cli/project.hpp>
#include "process.hpp"
#include <csignal>
#include <fstream>
#include <iostream>
#include <map>
#include <set>

namespace gungnir::cli {
namespace {
volatile std::sig_atomic_t stopped=0;
void interrupt(int signal) { stopped=signal; }
struct Signals {
    using Handler=void(*)(int);
    Handler previous_int,previous_term;
    Signals():previous_int(std::signal(SIGINT,interrupt)),previous_term(std::signal(SIGTERM,interrupt)) { stopped=0; }
    ~Signals() { std::signal(SIGINT,previous_int); std::signal(SIGTERM,previous_term); }
};
using Snapshot=std::map<std::string,std::uint64_t>;
Snapshot snapshot(const std::filesystem::path& root) {
    Snapshot result;
    for(auto it=std::filesystem::recursive_directory_iterator(root);it!=std::filesystem::recursive_directory_iterator{};++it) {
        const auto name=it->path().filename().string();
        if(it->is_directory() && (name==".gungnir" || name==".git" || name=="build" || name=="vendor" || name=="storage")) { it.disable_recursion_pending(); continue; }
        if(!it->is_regular_file()) continue;
        const auto relative=it->path().lexically_relative(root).generic_string();
        const auto extension=it->path().extension().string();
        if(extension!=".gnr" && extension!=".hpp" && extension!=".h" && extension!=".cpp" && extension!=".cmake" && name!=".env" && name!=".gungnir-project" && !relative.starts_with("views/") && !relative.starts_with("config/") && !relative.starts_with("bootstrap/")) continue;
        std::ifstream input(it->path(),std::ios::binary); if(!input) throw std::runtime_error("Unable to watch " + relative);
        std::uint64_t hash=14695981039346656037ULL; char c; while(input.get(c)) { hash^=static_cast<unsigned char>(c); hash*=1099511628211ULL; }
        result.emplace(relative,hash);
    }
    return result;
}
}
int Project::dev() const {
    Signals signals;
    detail::Child application;
    std::filesystem::path running;
    std::size_t generation=0;
    const auto rebuild=[&] {
        try {
            std::cerr << "gungnir dev: building\n";
            const auto code=build(false);
            if(code!=0) { std::cerr << "gungnir dev: build failed (" << code << "); waiting for changes\n"; return; }
            if(stopped) return;
            auto executable=root_ / ".gungnir/build/app";
#ifdef _WIN32
            executable=root_ / ".gungnir/build/Debug/app.exe";
            if(!std::filesystem::exists(executable)) executable=root_ / ".gungnir/build/app.exe";
#endif
            const auto next=root_ / ".gungnir/dev" / ("app-" + std::to_string(++generation) + executable.extension().string());
            std::filesystem::create_directories(next.parent_path());
            std::filesystem::copy_file(executable,next,std::filesystem::copy_options::overwrite_existing);
            application.stop();
            if(!running.empty()) { std::error_code ignored; std::filesystem::remove(running,ignored); }
            application.start({next.string()},root_,true); running=next;
            std::cerr << "gungnir dev: running; watching sources, bootstrap, configuration, and views\n";
        } catch(const std::exception& e) { std::cerr << "gungnir dev: " << e.what() << "\nWaiting for changes\n"; }
    };
    auto accepted=snapshot(root_), pending=accepted;
    rebuild();
    while(!stopped) {
        if(const auto code=application.poll()) std::cerr << "gungnir dev: application exited (" << *code << "); waiting for changes\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        try {
            auto current=snapshot(root_);
            if(current!=pending) { pending=std::move(current); continue; }
            if(pending!=accepted) { accepted=pending; rebuild(); }
        } catch(const std::exception& e) { std::cerr << "gungnir dev: " << e.what() << '\n'; }
    }
    application.stop();
    if(!running.empty()) { std::error_code ignored; std::filesystem::remove(running,ignored); }
    return 128+stopped;
}
}
