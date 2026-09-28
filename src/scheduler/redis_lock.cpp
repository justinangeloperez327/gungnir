#include <gungnir/scheduler/redis_lock.hpp>

#include <chrono>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <hiredis/hiredis.h>

#include <gungnir/security/random.hpp>

namespace gungnir::scheduler {

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
            "Redis scheduler lock timeout must be greater than zero"
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

constexpr std::string_view
release_script =
    "if redis.call('GET', KEYS[1]) ~= ARGV[1] then "
    "return 0 end "
    "return redis.call('DEL', KEYS[1])";

constexpr std::string_view
renew_script =
    "if redis.call('GET', KEYS[1]) ~= ARGV[1] then "
    "return 0 end "
    "return redis.call('PEXPIRE', KEYS[1], ARGV[2])";

} // namespace

class RedisLockStore::Impl {
public:
    explicit Impl(
        RedisLockSettings value
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
    std::optional<LockLease>
    acquire(
        std::string key,
        std::chrono::milliseconds ttl
    ) {
        validate_lock(
            key,
            ttl
        );

        std::lock_guard lock{
            mutex
        };

        const auto owner =
            security::random_token(
                24
            );

        const auto full_key =
            lock_key(key);

        auto reply =
            command({
                "SET",
                full_key,
                owner,
                "NX",
                "PX",
                std::to_string(
                    ttl.count()
                )
            });

        if (
            reply->type ==
            REDIS_REPLY_NIL
        ) {
            return std::nullopt;
        }

        if (
            reply->type !=
                REDIS_REPLY_STATUS ||
            reply_text(*reply) !=
                "OK"
        ) {
            throw std::runtime_error(
                "Redis scheduler lock SET returned an unexpected reply"
            );
        }

        return LockLease{
            std::move(key),
            owner
        };
    }

    [[nodiscard]]
    bool renew(
        const LockLease& lease,
        std::chrono::milliseconds ttl
    ) {
        validate_lock(
            lease.key,
            ttl
        );

        if (lease.owner.empty()) {
            return false;
        }

        std::lock_guard lock{
            mutex
        };

        auto reply =
            eval(
                renew_script,
                {
                    lock_key(
                        lease.key
                    )
                },
                {
                    lease.owner,
                    std::to_string(
                        ttl.count()
                    )
                }
            );

        require_integer(
            *reply,
            "renew"
        );

        return reply->integer == 1;
    }

    [[nodiscard]]
    bool release(
        const LockLease& lease
    ) {
        if (
            lease.key.empty() ||
            lease.owner.empty()
        ) {
            return false;
        }

        std::lock_guard lock{
            mutex
        };

        auto reply =
            eval(
                release_script,
                {
                    lock_key(
                        lease.key
                    )
                },
                {
                    lease.owner
                }
            );

        require_integer(
            *reply,
            "release"
        );

        return reply->integer == 1;
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

    void flush() {
        std::lock_guard lock{
            mutex
        };

        std::string cursor{"0"};

        do {
            auto reply =
                command({
                    "SCAN",
                    cursor,
                    "MATCH",
                    settings.prefix +
                        "*",
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
                    "Redis scheduler lock SCAN returned an unexpected reply"
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
                    "Redis scheduler lock SCAN key list has an unexpected reply type"
                );
            }

            if (keys->elements == 0) {
                continue;
            }

            std::vector<std::string>
                owned;

            owned.reserve(
                keys->elements + 1
            );

            owned.emplace_back(
                "DEL"
            );

            for (
                std::size_t index = 0;
                index < keys->elements;
                ++index
            ) {
                const auto* item =
                    keys->element[index];

                if (
                    item != nullptr &&
                    item->type ==
                        REDIS_REPLY_STRING
                ) {
                    owned.push_back(
                        reply_text(
                            *item
                        )
                    );
                }
            }

            if (owned.size() > 1) {
                std::vector<std::string_view>
                    views;

                views.reserve(
                    owned.size()
                );

                for (
                    const auto& value :
                    owned
                ) {
                    views.emplace_back(
                        value
                    );
                }

                auto deleted =
                    command(views);

                require_integer(
                    *deleted,
                    "flush"
                );
            }
        } while (cursor != "0");
    }

    RedisLockSettings settings;

private:
    void validate_settings()
        const {
        if (settings.host.empty()) {
            throw std::invalid_argument(
                "Redis scheduler lock host cannot be empty"
            );
        }

        if (settings.port == 0) {
            throw std::invalid_argument(
                "Redis scheduler lock port must be greater than zero"
            );
        }

        if (settings.database < 0) {
            throw std::invalid_argument(
                "Redis scheduler lock database cannot be negative"
            );
        }

        if (settings.prefix.empty()) {
            throw std::invalid_argument(
                "Redis scheduler lock prefix cannot be empty"
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

    static void validate_lock(
        const std::string& key,
        std::chrono::milliseconds ttl
    ) {
        if (key.empty()) {
            throw std::invalid_argument(
                "Redis scheduler lock key cannot be empty"
            );
        }

        if (ttl.count() <= 0) {
            throw std::invalid_argument(
                "Redis scheduler lock TTL must be greater than zero"
            );
        }
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
                "Unable to allocate Redis scheduler lock connection"
            );
        }

        if (context->err != 0) {
            const auto message =
                std::string{
                    context->errstr
                };

            disconnect();

            throw std::runtime_error(
                "Unable to connect to Redis scheduler lock store: " +
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
                "Unable to configure Redis scheduler lock timeout: " +
                message
            );
        }

        try {
            authenticate_unlocked();
            select_database_unlocked();
        } catch (...) {
            disconnect();
            throw;
        }
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

        if (
            reply->type !=
                REDIS_REPLY_STATUS ||
            reply_text(*reply) !=
                "OK"
        ) {
            throw std::runtime_error(
                "Redis scheduler lock AUTH failed"
            );
        }
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

        if (
            reply->type !=
                REDIS_REPLY_STATUS ||
            reply_text(*reply) !=
                "OK"
        ) {
            throw std::runtime_error(
                "Redis scheduler lock SELECT failed"
            );
        }
    }

    [[nodiscard]]
    Reply command(
        std::initializer_list<
            std::string_view
        > arguments
    ) {
        std::vector<std::string_view>
            values{
                arguments
            };

        return command(values);
    }

    [[nodiscard]]
    Reply command(
        const std::vector<
            std::string_view
        >& arguments
    ) {
        ensure_connected();

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
                "Redis scheduler lock command failed: " +
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
                "Redis scheduler lock command failed: " +
                reply_text(*reply)
            );
        }

        return reply;
    }

    [[nodiscard]]
    Reply eval(
        std::string_view script,
        const std::vector<
            std::string
        >& keys,
        const std::vector<
            std::string
        >& arguments
    ) {
        std::vector<std::string>
            owned;

        owned.reserve(
            3 +
            keys.size() +
            arguments.size()
        );

        owned.emplace_back(
            "EVAL"
        );

        owned.emplace_back(
            script
        );

        owned.push_back(
            std::to_string(
                keys.size()
            )
        );

        owned.insert(
            owned.end(),
            keys.begin(),
            keys.end()
        );

        owned.insert(
            owned.end(),
            arguments.begin(),
            arguments.end()
        );

        std::vector<std::string_view>
            views;

        views.reserve(
            owned.size()
        );

        for (
            const auto& value :
            owned
        ) {
            views.emplace_back(value);
        }

        return command(views);
    }

    void require_integer(
        const redisReply& reply,
        std::string_view operation
    ) const {
        if (
            reply.type !=
            REDIS_REPLY_INTEGER
        ) {
            throw std::runtime_error(
                "Redis scheduler lock " +
                std::string{operation} +
                " returned an unexpected reply"
            );
        }
    }

    [[nodiscard]]
    std::string lock_key(
        std::string_view key
    ) const {
        std::string result;
        result.reserve(
            settings.prefix.size() +
            key.size()
        );

        result +=
            settings.prefix;

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

RedisLockStore::RedisLockStore(
    RedisLockSettings settings
)
    : impl_(
        std::make_unique<Impl>(
            std::move(settings)
        )
      ) {}

RedisLockStore::~RedisLockStore() =
    default;

RedisLockStore::RedisLockStore(
    RedisLockStore&&
) noexcept = default;

RedisLockStore&
RedisLockStore::operator=(
    RedisLockStore&&
) noexcept = default;

std::optional<LockLease>
RedisLockStore::acquire(
    std::string key,
    std::chrono::milliseconds ttl
) {
    return
        impl_->acquire(
            std::move(key),
            ttl
        );
}

bool RedisLockStore::renew(
    const LockLease& lease,
    std::chrono::milliseconds ttl
) {
    return
        impl_->renew(
            lease,
            ttl
        );
}

bool RedisLockStore::release(
    const LockLease& lease
) {
    return
        impl_->release(
            lease
        );
}

bool RedisLockStore::ping() {
    return impl_->ping();
}

void RedisLockStore::flush() {
    impl_->flush();
}

const RedisLockSettings&
RedisLockStore::settings()
    const noexcept {
    return impl_->settings;
}

} // namespace gungnir::scheduler
