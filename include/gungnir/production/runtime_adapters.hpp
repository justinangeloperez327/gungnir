#pragma once

#include <string>

#include <gungnir/production/supervisor.hpp>

namespace gungnir {

class Application;

namespace queue {
class Worker;
}

namespace scheduler {
class Scheduler;
}

namespace production {

Supervisor& supervise(
    Supervisor& supervisor,
    Application& application,
    std::string name = "http"
);

Supervisor& supervise(
    Supervisor& supervisor,
    queue::Worker& worker,
    std::string name = "queue"
);

Supervisor& supervise(
    Supervisor& supervisor,
    scheduler::Scheduler& scheduler,
    std::string name = "scheduler"
);

} // namespace production
} // namespace gungnir
