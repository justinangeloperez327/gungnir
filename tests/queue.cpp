#include <cassert>
#include <stdexcept>

#include <gungnir/queue/queue.hpp>

int main() {
    using namespace gungnir;
    queue::MemoryDriver driver;
    queue::Worker worker{driver};

    int handled = 0;
    worker.handle("mail.send", [&](std::string_view payload) {
        assert(payload == "42");
        ++handled;
    });

    driver.push({"1", "mail.send", "42", 0, 3});
    assert(worker.run_one());
    assert(handled == 1);
    assert(driver.pending() == 0);
    assert(driver.failed() == 0);

    worker.handle("retry", [](std::string_view) {
        throw std::runtime_error{"failure"};
    });
    driver.push({"2", "retry", "", 0, 2});
    assert(worker.run_one());
    assert(driver.pending() == 1);
    assert(worker.run_one());
    assert(driver.failed() == 1);

    driver.push({"3", "unknown", "", 0, 1});
    assert(worker.run_one());
    assert(driver.failed() == 2);

    assert(!worker.run_one());
    return 0;
}
