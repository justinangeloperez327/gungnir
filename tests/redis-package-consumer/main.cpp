#include <cassert>
#include <cstdlib>
#include <string>

#include <gungnir/cache/redis_store.hpp>
#include <gungnir/queue/redis_driver.hpp>

int main() {
    gungnir::cache::RedisSettings settings;

    if (
        const auto* host =
            std::getenv(
                "GUNGNIR_REDIS_HOST"
            )
    ) {
        settings.host = host;
    }

    if (
        const auto* port =
            std::getenv(
                "GUNGNIR_REDIS_PORT"
            )
    ) {
        settings.port =
            static_cast<
                std::uint16_t
            >(
                std::stoi(port)
            );
    }

    settings.database = 15;
    settings.prefix =
        "gungnir:package:";

    gungnir::cache::RedisStore
        cache{
            settings
        };

    assert(cache.ping());

    cache.put(
        "key",
        "value"
    );

    const auto loaded =
        cache.get("key");

    assert(loaded);
    assert(
        *loaded ==
        "value"
    );

    cache.flush();

    assert(
        !cache.get("key")
    );

    gungnir::queue::RedisSettings
        queue_settings;

    queue_settings.host =
        settings.host;

    queue_settings.port =
        settings.port;

    queue_settings.database =
        settings.database;

    queue_settings.prefix =
        "gungnir:package:queue:";

    gungnir::queue::RedisDriver
        queue{
            queue_settings
        };

    assert(queue.ping());

    queue.flush();

    queue.push({
        "package-job",
        "package.test",
        "payload",
        0,
        1
    });

    const auto job =
        queue.pop();

    assert(job);
    assert(
        job->name ==
        "package.test"
    );

    assert(
        job->payload ==
        "payload"
    );

    queue.acknowledge(
        *job
    );

    assert(queue.pending() == 0);
    assert(queue.reserved() == 0);

    queue.flush();

    return 0;
}
