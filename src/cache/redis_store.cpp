#include <gungnir/cache/redis_store.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <hiredis/hiredis.h>

namespace gungnir::cache {

namespace {

using Reply =
    std::unique_ptr<
        redisReply,
        decltype(&freeReplyObject)
    >;

[[nodiscard]]
timeval timeout_value(
    std::chrono::milliseconds timeout
) {
    if (timeout.count() <= 0) {
        throw std::invalid_argument(
            "Redis timeout must be greater than zero"
        );
    }

    const auto seconds =
        std::chrono::duration_cast<
            std::chrono::seconds
        >(timeout);

    const auto remainder =
        timeout - seconds;

    timeval value{};
    value.tv_sec =
        static_cast<decltype(value.tv_sec)>(
            seconds.count()
        );
    value.tv_usec =
        static_cast<decltype(value.tv_usec)>(
            std::chrono::duration_cast<
                std::chrono::microseconds
            >(remainder).count()
        );

    return value;
}

[[nodiscard]]
std::string reply_text(
    const redisReply& reply
) {
    if (
        reply.str == nullptr ||
        reply.len == 0
    ) {
        return {};
    }

    return std::string{
        reply.str,
        reply.len
    };
}

void require_status(
    const redisReply& reply,
    std::string_view expected,
    std::string_view operation
) {
    if (
        reply.type !=
            REDIS_REPLY_STATUS ||
        reply_text(reply) !=
            expected
    ) {
        throw std::runtime_error(
            "Redis " +
            std::string{operation} +
            " returned an unexpected reply"
        );
    }
}

} // namespace

class RedisStore::Impl {
public:
    explicit Impl(
        RedisSettings value
    )
        : settings(
            std::move(value)
          ) {
        validate_settings();
    }

    ~Impl() {
        disconnect();
    }

    Impl(
        const Impl&
    ) = delete;

    Impl& operator=(
        const Impl&
    ) = delete;

    [[nodiscard]]
    std::optional<std::string> get(
        std::string_view key
    ) {
        std::lock_guard lock{
            mutex
        };

        const auto full_key =
            cache_key(key);

        auto reply =
            command({
                "GET",
                full_key
            });

        if (
            reply->type ==
            REDIS_REPLY_NIL
        ) {
            return std::nullopt;
        }

        if (
            reply->type !=
            REDIS_REPLY_STRING
        ) {
            throw std::runtime_error(
                "Redis GET returned an unexpected reply"
            );
        }

        return reply_text(
            *reply
        );
    }

    void put(
        std::string key,
        std::string value,
        std::optional<Duration> ttl
    ) {
        std::lock_guard lock{
            mutex
        };

        const auto full_key =
            cache_key(key);

        if (
            ttl &&
            ttl->count() <= 0
        ) {
            static_cast<void>(
                erase_unlocked(
                    full_key
                )
            );

            return;
        }

        Reply reply{
            nullptr,
            &freeReplyObject
        };

        if (ttl) {
            reply =
                command({
                    "SET",
                    full_key,
                    value,
                    "EX",
                    std::to_string(
                        ttl->count()
                    )
                });
        } else {
            reply =
                command({
                    "SET",
                    full_key,
                    value
                });
        }

        require_status(
            *reply,
            "OK",
            "SET"
        );
    }

    bool forget(
        std::string_view key
    ) {
        std::lock_guard lock{
            mutex
        };

        return erase_unlocked(
            cache_key(key)
        );
    }

    void flush() {
        std::lock_guard lock{
            mutex
        };

        if (settings.prefix.empty()) {
            auto reply =
                command({
                    "FLUSHDB"
                });

            require_status(
                *reply,
                "OK",
                "FLUSHDB"
            );

            return;
        }

        std::string cursor{"0"};

        do {
            auto reply =
                command({
                    "SCAN",
                    cursor,
                    "COUNT",
                    "256"
                });

            if (
                reply->type !=
                    REDIS_REPLY_ARRAY ||
                reply->elements != 2 ||
                reply->element[0] == nullptr ||
                reply->element[1] == nullptr
            ) {
                throw std::runtime_error(
                    "Redis SCAN returned an unexpected reply"
                );
            }

            cursor =
                reply_text(
                    *reply->element[0]
                );

            const auto* keys =
                reply->element[1];

            if (
                keys->type !=
                REDIS_REPLY_ARRAY
            ) {
                throw std::runtime_error(
                    "Redis SCAN key list has an unexpected reply type"
                );
            }

            std::vector<std::string>
                matching;

            matching.reserve(
                keys->elements
            );

            for (
                std::size_t index = 0;
                index < keys->elements;
                ++index
            ) {
                const auto* item =
                    keys->element[index];

                if (
                    item == nullptr ||
                    item->type !=
                        REDIS_REPLY_STRING
                ) {
                    continue;
                }

                auto candidate =
                    reply_text(
                        *item
                    );

                if (
                    candidate.starts_with(
                        settings.prefix
                    )
                ) {
                    matching.push_back(
                        std::move(candidate)
                    );
                }
            }

            erase_many_unlocked(
                matching
            );
        } while (cursor != "0");
    }

    [[nodiscard]]
    bool ping() {
        std::lock_guard lock{
            mutex
        };

        try {
            auto reply =
                command({
                    "PING"
                });

            return
                reply->type ==
                    REDIS_REPLY_STATUS &&
                reply_text(*reply) ==
                    "PONG";
        } catch (...) {
            return false;
        }
    }

    RedisSettings settings;

private:
    void validate_settings() const {
        if (settings.host.empty()) {
            throw std::invalid_argument(
                "Redis host cannot be empty"
            );
        }

        if (settings.port == 0) {
            throw std::invalid_argument(
                "Redis port must be greater than zero"
            );
        }

        if (settings.database < 0) {
            throw std::invalid_argument(
                "Redis database cannot be negative"
            );
        }

        static_cast<void>(
            timeout_value(
                settings.connect_timeout
            )
        );

        static_cast<void>(
            timeout_value(
                settings.command_timeout
            )
        );
    }

    void connect() {
        disconnect();

        auto connect_timeout =
            timeout_value(
                settings.connect_timeout
            );

        context =
            redisConnectWithTimeout(
                settings.host.c_str(),
                static_cast<int>(
                    settings.port
                ),
                connect_timeout
            );

        if (context == nullptr) {
            throw std::runtime_error(
                "Unable to allocate Redis connection"
            );
        }

        if (context->err != 0) {
            const auto message =
                std::string{
                    context->errstr
                };

            disconnect();

            throw std::runtime_error(
                "Unable to connect to Redis: " +
                message
            );
        }

        auto command_timeout =
            timeout_value(
                settings.command_timeout
            );

        if (
            redisSetTimeout(
                context,
                command_timeout
            ) != REDIS_OK
        ) {
            const auto message =
                std::string{
                    context->errstr
                };

            disconnect();

            throw std::runtime_error(
                "Unable to configure Redis command timeout: " +
                message
            );
        }

        authenticate_unlocked();
        select_database_unlocked();
    }

    void disconnect()
        noexcept {
        if (context != nullptr) {
            redisFree(context);
            context = nullptr;
        }
    }

    void ensure_connected() {
        if (
            context == nullptr ||
            context->err != 0
        ) {
            connect();
        }
    }

    void authenticate_unlocked() {
        if (!settings.password) {
            return;
        }

        Reply reply{
            nullptr,
            &freeReplyObject
        };

        if (
            settings.username &&
            !settings.username->empty()
        ) {
            reply =
                command({
                    "AUTH",
                    *settings.username,
                    *settings.password
                });
        } else {
            reply =
                command({
                    "AUTH",
                    *settings.password
                });
        }

        require_status(
            *reply,
            "OK",
            "AUTH"
        );
    }

    void select_database_unlocked() {
        if (settings.database == 0) {
            return;
        }

        auto reply =
            command({
                "SELECT",
                std::to_string(
                    settings.database
                )
            });

        require_status(
            *reply,
            "OK",
            "SELECT"
        );
    }

    [[nodiscard]]
    Reply command(
        std::initializer_list<
            std::string_view
        > arguments
    ) {
        std::vector<const char*>
            argv;

        std::vector<std::size_t>
            lengths;

        argv.reserve(
            arguments.size()
        );

        lengths.reserve(
            arguments.size()
        );

        for (
            const auto argument :
            arguments
        ) {
            argv.push_back(
                argument.data()
            );

            lengths.push_back(
                argument.size()
            );
        }

        return command_argv(
            argv,
            lengths
        );
    }

    [[nodiscard]]
    Reply command_argv(
        const std::vector<
            const char*
        >& argv,
        const std::vector<
            std::size_t
        >& lengths
    ) {
        ensure_connected();

        auto* raw =
            static_cast<redisReply*>(
                redisCommandArgv(
                    context,
                    static_cast<int>(
                        argv.size()
                    ),
                    argv.data(),
                    lengths.data()
                )
            );

        if (raw == nullptr) {
            const auto message =
                context != nullptr
                    ? std::string{
                        context->errstr
                      }
                    : std::string{
                        "connection unavailable"
                      };

            disconnect();

            throw std::runtime_error(
                "Redis command failed: " +
                message
            );
        }

        Reply reply{
            raw,
            &freeReplyObject
        };

        if (
            reply->type ==
            REDIS_REPLY_ERROR
        ) {
            throw std::runtime_error(
                "Redis command failed: " +
                reply_text(*reply)
            );
        }

        return reply;
    }

    [[nodiscard]]
    bool erase_unlocked(
        std::string_view key
    ) {
        auto reply =
            command({
                "DEL",
                key
            });

        if (
            reply->type !=
            REDIS_REPLY_INTEGER
        ) {
            throw std::runtime_error(
                "Redis DEL returned an unexpected reply"
            );
        }

        return reply->integer > 0;
    }

    void erase_many_unlocked(
        const std::vector<
            std::string
        >& keys
    ) {
        if (keys.empty()) {
            return;
        }

        std::vector<const char*>
            argv;

        std::vector<std::size_t>
            lengths;

        argv.reserve(
            keys.size() + 1
        );

        lengths.reserve(
            keys.size() + 1
        );

        static constexpr char
            command_name[] =
                "DEL";

        argv.push_back(
            command_name
        );

        lengths.push_back(3);

        for (const auto& key : keys) {
            argv.push_back(
                key.data()
            );

            lengths.push_back(
                key.size()
            );
        }

        auto reply =
            command_argv(
                argv,
                lengths
            );

        if (
            reply->type !=
            REDIS_REPLY_INTEGER
        ) {
            throw std::runtime_error(
                "Redis DEL batch returned an unexpected reply"
            );
        }
    }

    [[nodiscard]]
    std::string cache_key(
        std::string_view key
    ) const {
        std::string result;
        result.reserve(
            settings.prefix.size() +
            key.size()
        );

        result += settings.prefix;
        result.append(
            key.data(),
            key.size()
        );

        return result;
    }

    redisContext* context{
        nullptr
    };

    std::mutex mutex;
};

RedisStore::RedisStore(
    RedisSettings settings
)
    : impl_(
        std::make_unique<Impl>(
            std::move(settings)
        )
      ) {}

RedisStore::~RedisStore() =
    default;

RedisStore::RedisStore(
    RedisStore&&
) noexcept = default;

RedisStore& RedisStore::operator=(
    RedisStore&&
) noexcept = default;

std::optional<std::string>
RedisStore::get(
    std::string_view key
) {
    return impl_->get(key);
}

void RedisStore::put(
    std::string key,
    std::string value,
    std::optional<Duration> ttl
) {
    impl_->put(
        std::move(key),
        std::move(value),
        ttl
    );
}

bool RedisStore::forget(
    std::string_view key
) {
    return impl_->forget(key);
}

void RedisStore::flush() {
    impl_->flush();
}

bool RedisStore::ping() {
    return impl_->ping();
}

const RedisSettings&
RedisStore::settings()
    const noexcept {
    return impl_->settings;
}

} // namespace gungnir::cache
