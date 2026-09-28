#include <gungnir/production/runtime_adapters.hpp>

#include <utility>

#include <gungnir/core/application.hpp>
#include <gungnir/queue/worker.hpp>
#include <gungnir/scheduler/scheduler.hpp>

namespace gungnir::production {

Supervisor& supervise(
    Supervisor& supervisor,
    Application& application,
    std::string name
) {
    return supervisor.runtime(
        std::move(name),
        [&application] {
            application.stop();
        },
        [&application] {
            return application.is_running();
        }
    );
}

Supervisor& supervise(
    Supervisor& supervisor,
    queue::Worker& worker,
    std::string name
) {
    return supervisor.runtime(
        std::move(name),
        [&worker] {
            worker.request_stop();
        },
        [&worker] {
            return worker.running();
        }
    );
}

Supervisor& supervise(
    Supervisor& supervisor,
    scheduler::Scheduler& scheduler,
    std::string name
) {
    return supervisor.runtime(
        std::move(name),
        [&scheduler] {
            scheduler.request_stop();
        },
        [&scheduler] {
            return scheduler.running();
        }
    );
}

} // namespace gungnir::production
