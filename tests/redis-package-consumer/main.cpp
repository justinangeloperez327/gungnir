#include <cassert>
#include <chrono>
#include <cstdlib>
#include <string>

#include <gungnir/cache/redis_store.hpp>
#include <gungnir/queue/redis_driver.hpp>
#include <gungnir/session/redis_store.hpp>
#include <gungnir/security/random.hpp>

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

    gungnir::session::
        RedisSessionSettings
        session_settings;

    session_settings.redis.host =
        settings.host;

    session_settings.redis.port =
        settings.port;

    session_settings.redis.database =
        settings.database;

    session_settings.redis.prefix =
        "gungnir:package:session:";

    session_settings.lifetime =
        std::chrono::seconds{60};

    gungnir::session::RedisStore
        sessions{
            session_settings
        };

    assert(sessions.ping());

    sessions.flush();

    const auto session_id =
        gungnir::security::
            random_token();

    gungnir::session::Session
        session{
            session_id
        };

    session.put(
        "user_id",
        "42"
    );

    sessions.save(session);

    const auto loaded_session =
        sessions.load(
            session_id
        );

    assert(loaded_session);

    assert(
        loaded_session->get(
            "user_id"
        ) == "42"
    );

    sessions.erase(
        session_id
    );

    assert(
        !sessions.load(
            session_id
        )
    );

    sessions.flush();

    return 0;
}
