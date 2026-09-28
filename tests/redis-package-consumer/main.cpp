#include <cassert>
#include <cstdlib>
#include <string>

#include <gungnir/cache/redis_store.hpp>

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

    return 0;
}
