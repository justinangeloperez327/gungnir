#include "process.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef _WIN32
#include <cerrno>

using gungnir::cli::detail::Child;

void require(bool condition,const char* message) {
    if(!condition) throw std::runtime_error(message);
}

void expect_launch_failure(const std::vector<std::string>& arguments,
                           const std::filesystem::path& cwd={}) {
    Child child;
    bool failed=false;
    try {
        child.start(arguments,cwd,true);
    } catch(const std::runtime_error& error) {
        const std::string diagnostic=error.what();
        failed=diagnostic.find("Unable to start")!=std::string::npos &&
               diagnostic.find("errno ")!=std::string::npos;
    }
    require(failed,"Failed exec/chdir must report synchronous launch failure");
    require(!child.poll().has_value(),"Failed launch must be fully reaped");
}

int main() {
    // Fork succeeded, but execvp must fail synchronously with ENOENT.
    expect_launch_failure({"/gungnir-not-a-real-program/does-not-exist"});

    // A valid executable and invalid working directory must fail before the
    // caller mistakes this child for a running HTTP application.
    expect_launch_failure({"/bin/sh","-c","exit 0"},
                          "/gungnir-not-a-real-working-directory");

    // Real subprocesses still produce their own exit codes; a nonzero
    // application exit is not a launch error.
    Child exited;
    exited.start({"/bin/sh","-c","exit 17"},{},true);
    require(exited.wait()==17,"A launched nonzero child must retain its exit status");

    // The class must remain reusable after a synchronous launch exception.
    Child recovered;
    bool launch_failed=false;
    try {
        recovered.start({"/gungnir-not-a-real-program/does-not-exist"});
    } catch(const std::runtime_error&) { launch_failed=true; }
    require(launch_failed,"Invalid command unexpectedly launched");
    recovered.start({"/bin/sh","-c","exit 0"});
    require(recovered.wait()==0,"Child must be reusable after a failed launch");
}
#else
int main() { return 0; }
#endif
