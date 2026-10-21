#include "process.hpp"

#include <cassert>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef _WIN32
#include <cerrno>

using gungnir::cli::detail::Child;

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
    assert(failed && "Failed exec/chdir must report synchronous launch failure");
    assert(!child.poll().has_value() && "Failed launch must be fully reaped");
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
    assert(exited.wait()==17);

    // The class must remain reusable after a synchronous launch exception.
    Child recovered;
    try {
        recovered.start({"/gungnir-not-a-real-program/does-not-exist"});
        assert(false && "Invalid command unexpectedly launched");
    } catch(const std::runtime_error&) {}
    recovered.start({"/bin/sh","-c","exit 0"});
    assert(recovered.wait()==0);
}
#else
int main() { return 0; }
#endif
