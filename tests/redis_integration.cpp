#include <cassert>
#include <chrono>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <gungnir/cache/redis_store.hpp>
#include <gungnir/cache/repository.hpp>
#include <gungnir/queue/redis_driver.hpp>
#include <gungnir/queue/worker.hpp>

namespace {

std::string env(
    const char* name,
    std::string fallback
) {
    if (
        const auto* value =
            std::getenv(name)
    ) {
        return value;
    }

    return fallback;
}

std::uint16_t redis_port() {
    return static_cast<std::uint16_t>(
        std::stoi(
            env(
                "GUNGNIR_REDIS_PORT",
                "6379"
            )
        )
    );
}

gungnir::cache::RedisSettings settings(
    std::string prefix
) {
    gungnir::cache::RedisSettings value;
    value.host =
        env(
            "GUNGNIR_REDIS_HOST",
            "127.0.0.1"
        );
    value.port = redis_port();
    value.database = 15;
    value.prefix =
        std::move(prefix);

    return value;
}

} // namespace

int main() {
    using namespace gungnir;

    cache::RedisStore outside{
        settings("")
    };

    cache::RedisStore store{
        settings(
            "gungnir:cache:test:"
        )
    };

    assert(outside.ping());
    assert(store.ping());

    outside.flush();

    cache::Repository cache{
        store
    };

    assert(
        !cache.get("missing")
    );

    cache.put(
        "name",
        "Gungnir"
    );

    assert(
        cache.get("name") ==
        std::optional<std::string>{
            "Gungnir"
        }
    );

    const std::string binary{
        "a\0b\0c",
        5
    };

    cache.put(
        "binary",
        binary
    );

    const auto binary_loaded =
        cache.get("binary");

    assert(binary_loaded);
    assert(
        *binary_loaded ==
        binary
    );

    cache.put(
        "temporary",
        "value",
        std::chrono::seconds{1}
    );

    assert(
        cache.has("temporary")
    );

    std::this_thread::sleep_for(
        std::chrono::milliseconds{
            1250
        }
    );

    assert(
        !cache.has("temporary")
    );

    cache.put(
        "zero",
        "value",
        std::chrono::seconds{0}
    );

    assert(
        !cache.has("zero")
    );

    int calls = 0;

    const auto first =
        cache.remember(
            "computed",
            std::chrono::seconds{60},
            [&] {
                ++calls;
                return std::string{
                    "value"
                };
            }
        );

    const auto second =
        cache.remember(
            "computed",
            std::chrono::seconds{60},
            [&] {
                ++calls;
                return std::string{
                    "other"
                };
            }
        );

    assert(first == "value");
    assert(second == "value");
    assert(calls == 1);

    assert(
        cache.forget("name")
    );

    assert(
        !cache.has("name")
    );

    outside.put(
        "outside",
        "preserved"
    );

    cache.put(
        "inside-a",
        "1"
    );

    cache.put(
        "inside-b",
        "2"
    );

    cache.flush();

    assert(
        !cache.has("inside-a")
    );

    assert(
        !cache.has("inside-b")
    );

    assert(
        outside.get("outside") ==
        std::optional<std::string>{
            "preserved"
        }
    );

    std::vector<std::thread>
        workers;

    for (
        int worker = 0;
        worker < 8;
        ++worker
    ) {
        workers.emplace_back(
            [&store, worker] {
                for (
                    int item = 0;
                    item < 40;
                    ++item
                ) {
                    const auto key =
                        "worker-" +
                        std::to_string(
                            worker
                        ) +
                        "-" +
                        std::to_string(
                            item
                        );

                    store.put(
                        key,
                        "value"
                    );

                    const auto loaded =
                        store.get(key);

                    assert(loaded);
                    assert(
                        *loaded ==
                        "value"
                    );
                }
            }
        );
    }

    for (
        auto& worker :
        workers
    ) {
        worker.join();
    }

    cache.flush();

    outside.forget(
        "outside"
    );

    queue::RedisSettings
        queue_settings;

    queue_settings.host =
        env(
            "GUNGNIR_REDIS_HOST",
            "127.0.0.1"
        );

    queue_settings.port =
        redis_port();

    queue_settings.database = 14;
    queue_settings.prefix =
        "gungnir:queue:test:";

    queue_settings.queue =
        "default";

    queue_settings.visibility_timeout =
        std::chrono::milliseconds{
            120
        };

    queue::RedisDriver queue{
        queue_settings
    };

    assert(queue.ping());

    queue.flush();

    const std::string
        queue_binary_payload{
            "a\0b",
            3
        };

    queue.push({
        "job-1",
        "mail.send",
        queue_binary_payload,
        0,
        3
    });

    assert(queue.pending() == 1);
    assert(queue.reserved() == 0);

    auto first_lease =
        queue.pop();

    assert(first_lease);
    assert(
        first_lease->id ==
        "job-1"
    );

    assert(
        first_lease->payload ==
        queue_binary_payload
    );

    assert(
        !first_lease
            ->reservation
            .empty()
    );

    const auto stale =
        *first_lease;

    assert(queue.pending() == 0);
    assert(queue.reserved() == 1);

    std::this_thread::sleep_for(
        std::chrono::milliseconds{
            180
        }
    );

    auto recovered =
        queue.pop();

    assert(recovered);

    assert(
        recovered->id ==
        "job-1"
    );

    assert(
        recovered->reservation !=
        stale.reservation
    );

    queue.acknowledge(
        stale
    );

    assert(queue.reserved() == 1);

    queue.acknowledge(
        *recovered
    );

    assert(queue.pending() == 0);
    assert(queue.reserved() == 0);

    bool duplicate_rejected = false;

    queue.push({
        "job-2",
        "retry",
        "payload",
        0,
        2
    });

    try {
        queue.push({
            "job-2",
            "retry",
            "other",
            0,
            2
        });
    } catch (
        const std::logic_error&
    ) {
        duplicate_rejected = true;
    }

    assert(duplicate_rejected);

    queue::Worker worker{
        queue
    };

    worker.handle(
        "retry",
        [](
            std::string_view
        ) {
            throw std::runtime_error{
                "failure"
            };
        }
    );

    assert(worker.run_one());
    assert(queue.pending() == 1);
    assert(queue.reserved() == 0);
    assert(queue.failed() == 0);

    assert(worker.run_one());
    assert(queue.pending() == 0);
    assert(queue.reserved() == 0);
    assert(queue.failed() == 1);

    queue.flush();

    assert(queue.pending() == 0);
    assert(queue.reserved() == 0);
    assert(queue.failed() == 0);

    return 0;
}
