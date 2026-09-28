#include <gungnir/queue/redis_driver.hpp>

#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <hiredis/hiredis.h>

#include <gungnir/security/random.hpp>

namespace gungnir::queue {

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
            "Redis queue timeout must be greater than zero"
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

void append_field(
    std::string& output,
    std::string_view value
) {
    output +=
        std::to_string(
            value.size()
        );

    output.push_back(':');

    output.append(
        value.data(),
        value.size()
    );
}

[[nodiscard]]
std::string_view read_field(
    std::string_view input,
    std::size_t& cursor
) {
    const auto separator =
        input.find(
            ':',
            cursor
        );

    if (
        separator ==
        std::string_view::npos
    ) {
        throw std::runtime_error(
            "Invalid Redis queue envelope"
        );
    }

    std::size_t length = 0;

    const auto begin =
        input.data() +
        static_cast<
            std::ptrdiff_t
        >(cursor);

    const auto end =
        input.data() +
        static_cast<
            std::ptrdiff_t
        >(separator);

    const auto parsed =
        std::from_chars(
            begin,
            end,
            length
        );

    if (
        parsed.ec !=
            std::errc{} ||
        parsed.ptr != end
    ) {
        throw std::runtime_error(
            "Invalid Redis queue envelope length"
        );
    }

    cursor =
        separator + 1;

    if (
        length >
        input.size() - cursor
    ) {
        throw std::runtime_error(
            "Truncated Redis queue envelope"
        );
    }

    const auto value =
        input.substr(
            cursor,
            length
        );

    cursor += length;

    return value;
}

[[nodiscard]]
unsigned read_unsigned(
    std::string_view value
) {
    unsigned output = 0;

    const auto parsed =
        std::from_chars(
            value.data(),
            value.data() +
                static_cast<
                    std::ptrdiff_t
                >(value.size()),
            output
        );

    if (
        parsed.ec !=
            std::errc{} ||
        parsed.ptr !=
            value.data() +
                static_cast<
                    std::ptrdiff_t
                >(value.size())
    ) {
        throw std::runtime_error(
            "Invalid Redis queue attempt count"
        );
    }

    return output;
}

[[nodiscard]]
std::string encode(
    const Envelope& job
) {
    std::string output;

    output.reserve(
        job.id.size() +
        job.name.size() +
        job.payload.size() +
        64
    );

    append_field(
        output,
        job.id
    );

    append_field(
        output,
        job.name
    );

    append_field(
        output,
        job.payload
    );

    const auto attempts =
        std::to_string(
            job.attempts
        );

    append_field(
        output,
        attempts
    );

    const auto maximum =
        std::to_string(
            job.max_attempts
        );

    append_field(
        output,
        maximum
    );

    return output;
}

[[nodiscard]]
Envelope decode(
    std::string_view encoded
) {
    std::size_t cursor = 0;

    Envelope job;

    job.id =
        std::string{
            read_field(
                encoded,
                cursor
            )
        };

    job.name =
        std::string{
            read_field(
                encoded,
                cursor
            )
        };

    job.payload =
        std::string{
            read_field(
                encoded,
                cursor
            )
        };

    job.attempts =
        read_unsigned(
            read_field(
                encoded,
                cursor
            )
        );

    job.max_attempts =
        read_unsigned(
            read_field(
                encoded,
                cursor
            )
        );

    if (
        cursor !=
        encoded.size()
    ) {
        throw std::runtime_error(
            "Redis queue envelope contains trailing data"
        );
    }

    return job;
}

void validate_job(
    const Envelope& job
) {
    if (job.id.empty()) {
        throw std::invalid_argument(
            "Queued job id cannot be empty"
        );
    }

    if (job.name.empty()) {
        throw std::invalid_argument(
            "Queued job name cannot be empty"
        );
    }

    if (job.max_attempts == 0) {
        throw std::invalid_argument(
            "Queued job max_attempts must be greater than zero"
        );
    }
}

constexpr std::string_view push_script =
    "if redis.call('HEXISTS', KEYS[1], ARGV[1]) == 1 "
    "or redis.call('HEXISTS', KEYS[3], ARGV[1]) == 1 then "
    "return 0 end "
    "redis.call('HSET', KEYS[1], ARGV[1], ARGV[2]) "
    "redis.call('RPUSH', KEYS[2], ARGV[1]) "
    "return 1";

constexpr std::string_view push_later_script =
    "if redis.call('HEXISTS', KEYS[1], ARGV[1]) == 1 "
    "or redis.call('HEXISTS', KEYS[3], ARGV[1]) == 1 then "
    "return 0 end "
    "redis.call('HSET', KEYS[1], ARGV[1], ARGV[2]) "
    "local delay = tonumber(ARGV[3]) "
    "if delay <= 0 then "
    "redis.call('RPUSH', KEYS[2], ARGV[1]) "
    "else "
    "local t = redis.call('TIME') "
    "local now = tonumber(t[1]) * 1000 + math.floor(tonumber(t[2]) / 1000) "
    "redis.call('ZADD', KEYS[4], now + delay, ARGV[1]) "
    "end "
    "return 1";

constexpr std::string_view pop_script =
    "local t = redis.call('TIME') "
    "local now = tonumber(t[1]) * 1000 + math.floor(tonumber(t[2]) / 1000) "
    "local due = redis.call('ZRANGEBYSCORE', KEYS[5], '-inf', now) "
    "for _, id in ipairs(due) do "
    "redis.call('ZREM', KEYS[5], id) "
    "if redis.call('HEXISTS', KEYS[1], id) == 1 then "
    "redis.call('RPUSH', KEYS[2], id) "
    "end "
    "end "
    "local expired = redis.call('ZRANGEBYSCORE', KEYS[3], '-inf', now) "
    "for _, id in ipairs(expired) do "
    "redis.call('ZREM', KEYS[3], id) "
    "redis.call('HDEL', KEYS[4], id) "
    "if redis.call('HEXISTS', KEYS[1], id) == 1 then "
    "redis.call('RPUSH', KEYS[2], id) "
    "end "
    "end "
    "while true do "
    "local id = redis.call('LPOP', KEYS[2]) "
    "if not id then return nil end "
    "local payload = redis.call('HGET', KEYS[1], id) "
    "if payload then "
    "redis.call('HSET', KEYS[4], id, ARGV[2]) "
    "redis.call('ZADD', KEYS[3], now + tonumber(ARGV[1]), id) "
    "return {id, payload} "
    "end "
    "end";

constexpr std::string_view acknowledge_script =
    "local token = redis.call('HGET', KEYS[3], ARGV[1]) "
    "if not token or token ~= ARGV[2] then return 0 end "
    "redis.call('HDEL', KEYS[3], ARGV[1]) "
    "redis.call('ZREM', KEYS[2], ARGV[1]) "
    "redis.call('HDEL', KEYS[1], ARGV[1]) "
    "return 1";

constexpr std::string_view release_after_script =
    "local token = redis.call('HGET', KEYS[4], ARGV[1]) "
    "if not token or token ~= ARGV[2] then return 0 end "
    "redis.call('HSET', KEYS[1], ARGV[1], ARGV[3]) "
    "redis.call('HDEL', KEYS[4], ARGV[1]) "
    "redis.call('ZREM', KEYS[3], ARGV[1]) "
    "local delay = tonumber(ARGV[4]) "
    "if delay <= 0 then "
    "redis.call('RPUSH', KEYS[2], ARGV[1]) "
    "else "
    "local t = redis.call('TIME') "
    "local now = tonumber(t[1]) * 1000 + math.floor(tonumber(t[2]) / 1000) "
    "redis.call('ZADD', KEYS[5], now + delay, ARGV[1]) "
    "end "
    "return 1";

constexpr std::string_view renew_script =
    "local token = redis.call('HGET', KEYS[2], ARGV[1]) "
    "if not token or token ~= ARGV[2] then return 0 end "
    "local t = redis.call('TIME') "
    "local now = tonumber(t[1]) * 1000 + math.floor(tonumber(t[2]) / 1000) "
    "redis.call('ZADD', KEYS[1], now + tonumber(ARGV[3]), ARGV[1]) "
    "return 1";

constexpr std::string_view fail_script =
    "local token = redis.call('HGET', KEYS[4], ARGV[1]) "
    "if not token or token ~= ARGV[2] then return 0 end "
    "redis.call('HSET', KEYS[2], ARGV[1], ARGV[3]) "
    "redis.call('HDEL', KEYS[1], ARGV[1]) "
    "redis.call('HDEL', KEYS[4], ARGV[1]) "
    "redis.call('ZREM', KEYS[3], ARGV[1]) "
    "return 1";

constexpr std::string_view retry_failed_script =
    "local current = redis.call('HGET', KEYS[3], ARGV[1]) "
    "if not current or current ~= ARGV[2] then return 0 end "
    "if redis.call('HEXISTS', KEYS[1], ARGV[1]) == 1 then return -1 end "
    "redis.call('HDEL', KEYS[3], ARGV[1]) "
    "redis.call('HSET', KEYS[1], ARGV[1], ARGV[3]) "
    "redis.call('RPUSH', KEYS[2], ARGV[1]) "
    "return 1";

} // namespace

class RedisDriver::Impl {
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

    void push(
        Envelope job
    ) {
        validate_job(job);
        job.reservation.clear();

        std::lock_guard lock{
            mutex
        };

        const auto payload =
            encode(job);

        auto reply =
            eval(
                push_script,
                {
                    jobs_key(),
                    ready_key(),
                    failed_key()
                },
                {
                    job.id,
                    payload
                }
            );

        require_integer(
            *reply,
            "queue push"
        );

        if (reply->integer == 0) {
            throw std::logic_error(
                "Queued job id already exists: " +
                job.id
            );
        }
    }

    void push_later(
        Envelope job,
        std::chrono::milliseconds delay
    ) {
        validate_job(job);
        job.reservation.clear();

        std::lock_guard lock{
            mutex
        };

        const auto payload =
            encode(job);

        auto reply =
            eval(
                push_later_script,
                {
                    jobs_key(),
                    ready_key(),
                    failed_key(),
                    delayed_key()
                },
                {
                    job.id,
                    payload,
                    std::to_string(
                        delay.count()
                    )
                }
            );

        require_integer(
            *reply,
            "queue delayed push"
        );

        if (reply->integer == 0) {
            throw std::logic_error(
                "Queued job id already exists: " +
                job.id
            );
        }
    }

    [[nodiscard]]
    std::optional<Envelope>
    pop() {
        std::lock_guard lock{
            mutex
        };

        const auto token =
            security::random_token(
                16
            );

        auto reply =
            eval(
                pop_script,
                {
                    jobs_key(),
                    ready_key(),
                    reserved_key(),
                    leases_key(),
                    delayed_key()
                },
                {
                    std::to_string(
                        settings
                            .visibility_timeout
                            .count()
                    ),
                    token
                }
            );

        if (
            reply->type ==
            REDIS_REPLY_NIL
        ) {
            return std::nullopt;
        }

        if (
            reply->type !=
                REDIS_REPLY_ARRAY ||
            reply->elements != 2 ||
            reply->element[0] == nullptr ||
            reply->element[1] == nullptr ||
            reply->element[0]->type !=
                REDIS_REPLY_STRING ||
            reply->element[1]->type !=
                REDIS_REPLY_STRING
        ) {
            throw std::runtime_error(
                "Redis queue pop returned an unexpected reply"
            );
        }

        auto job =
            decode(
                reply_text(
                    *reply->element[1]
                )
            );

        const auto id =
            reply_text(
                *reply->element[0]
            );

        if (job.id != id) {
            throw std::runtime_error(
                "Redis queue job identity mismatch"
            );
        }

        job.reservation =
            token;

        return job;
    }

    void acknowledge(
        const Envelope& job
    ) {
        if (
            job.id.empty() ||
            job.reservation.empty()
        ) {
            return;
        }

        std::lock_guard lock{
            mutex
        };

        auto reply =
            eval(
                acknowledge_script,
                {
                    jobs_key(),
                    reserved_key(),
                    leases_key()
                },
                {
                    job.id,
                    job.reservation
                }
            );

        require_integer(
            *reply,
            "queue acknowledge"
        );
    }

    void release(
        Envelope job
    ) {
        release_after(
            std::move(job),
            std::chrono::milliseconds{0}
        );
    }

    void release_after(
        Envelope job,
        std::chrono::milliseconds delay
    ) {
        validate_job(job);

        if (job.reservation.empty()) {
            throw std::invalid_argument(
                "Released Redis job has no reservation"
            );
        }

        std::lock_guard lock{
            mutex
        };

        const auto reservation =
            job.reservation;

        job.reservation.clear();

        const auto payload =
            encode(job);

        auto reply =
            eval(
                release_after_script,
                {
                    jobs_key(),
                    ready_key(),
                    reserved_key(),
                    leases_key(),
                    delayed_key()
                },
                {
                    job.id,
                    reservation,
                    payload,
                    std::to_string(
                        delay.count()
                    )
                }
            );

        require_integer(
            *reply,
            "queue release"
        );
    }

    [[nodiscard]]
    bool renew(
        const Envelope& job
    ) {
        if (
            job.id.empty() ||
            job.reservation.empty()
        ) {
            return false;
        }

        std::lock_guard lock{
            mutex
        };

        auto reply =
            eval(
                renew_script,
                {
                    reserved_key(),
                    leases_key()
                },
                {
                    job.id,
                    job.reservation,
                    std::to_string(
                        settings
                            .visibility_timeout
                            .count()
                    )
                }
            );

        require_integer(
            *reply,
            "queue lease renewal"
        );

        return reply->integer == 1;
    }

    void fail(
        const Envelope& job
    ) {
        validate_job(job);

        if (job.reservation.empty()) {
            return;
        }

        std::lock_guard lock{
            mutex
        };

        auto persisted =
            job;

        persisted.reservation.clear();

        const auto payload =
            encode(persisted);

        auto reply =
            eval(
                fail_script,
                {
                    jobs_key(),
                    failed_key(),
                    reserved_key(),
                    leases_key()
                },
                {
                    job.id,
                    job.reservation,
                    payload
                }
            );

        require_integer(
            *reply,
            "queue fail"
        );
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

    [[nodiscard]]
    std::size_t pending() {
        std::lock_guard lock{
            mutex
        };

        return count(
            "LLEN",
            ready_key()
        );
    }

    [[nodiscard]]
    std::size_t reserved() {
        std::lock_guard lock{
            mutex
        };

        return count(
            "ZCARD",
            reserved_key()
        );
    }

    [[nodiscard]]
    std::size_t delayed() {
        std::lock_guard lock{
            mutex
        };

        return count(
            "ZCARD",
            delayed_key()
        );
    }

    [[nodiscard]]
    std::size_t failed() {
        std::lock_guard lock{
            mutex
        };

        return count(
            "HLEN",
            failed_key()
        );
    }

    [[nodiscard]]
    std::vector<Envelope>
    failed_jobs() {
        std::lock_guard lock{
            mutex
        };

        auto reply =
            command({
                "HVALS",
                failed_key()
            });

        if (
            reply->type !=
            REDIS_REPLY_ARRAY
        ) {
            throw std::runtime_error(
                "Redis queue failed-job listing returned an unexpected reply"
            );
        }

        std::vector<Envelope> jobs;
        jobs.reserve(
            reply->elements
        );

        for (
            std::size_t index = 0;
            index < reply->elements;
            ++index
        ) {
            const auto* item =
                reply->element[index];

            if (
                item == nullptr ||
                item->type !=
                    REDIS_REPLY_STRING
            ) {
                throw std::runtime_error(
                    "Redis queue failed-job listing contains an invalid entry"
                );
            }

            jobs.push_back(
                decode(
                    reply_text(
                        *item
                    )
                )
            );
        }

        return jobs;
    }

    [[nodiscard]]
    std::optional<Envelope>
    failed_job(
        std::string_view id
    ) {
        if (id.empty()) {
            return std::nullopt;
        }

        std::lock_guard lock{
            mutex
        };

        auto reply =
            command({
                "HGET",
                failed_key(),
                id
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
                "Redis queue failed-job lookup returned an unexpected reply"
            );
        }

        return decode(
            reply_text(
                *reply
            )
        );
    }

    bool retry_failed(
        std::string_view id
    ) {
        if (id.empty()) {
            return false;
        }

        std::lock_guard lock{
            mutex
        };

        auto current =
            command({
                "HGET",
                failed_key(),
                id
            });

        if (
            current->type ==
            REDIS_REPLY_NIL
        ) {
            return false;
        }

        if (
            current->type !=
            REDIS_REPLY_STRING
        ) {
            throw std::runtime_error(
                "Redis queue failed-job retry lookup returned an unexpected reply"
            );
        }

        const auto persisted =
            reply_text(
                *current
            );

        auto job =
            decode(
                persisted
            );

        job.attempts = 0;
        job.reservation.clear();

        const auto reset =
            encode(job);

        auto reply =
            eval(
                retry_failed_script,
                {
                    jobs_key(),
                    ready_key(),
                    failed_key()
                },
                {
                    std::string{id},
                    persisted,
                    reset
                }
            );

        require_integer(
            *reply,
            "queue failed-job retry"
        );

        if (reply->integer < 0) {
            throw std::logic_error(
                "Cannot retry failed job because its id is already active: " +
                std::string{id}
            );
        }

        return reply->integer == 1;
    }

    bool forget_failed(
        std::string_view id
    ) {
        if (id.empty()) {
            return false;
        }

        std::lock_guard lock{
            mutex
        };

        auto reply =
            command({
                "HDEL",
                failed_key(),
                id
            });

        require_integer(
            *reply,
            "queue failed-job delete"
        );

        return reply->integer == 1;
    }

    void flush() {
        std::lock_guard lock{
            mutex
        };

        auto reply =
            command({
                "DEL",
                jobs_key(),
                ready_key(),
                reserved_key(),
                leases_key(),
                delayed_key(),
                failed_key()
            });

        require_integer(
            *reply,
            "queue flush"
        );
    }

    RedisSettings settings;

private:
    void validate_settings()
        const {
        if (settings.host.empty()) {
            throw std::invalid_argument(
                "Redis queue host cannot be empty"
            );
        }

        if (settings.port == 0) {
            throw std::invalid_argument(
                "Redis queue port must be greater than zero"
            );
        }

        if (settings.database < 0) {
            throw std::invalid_argument(
                "Redis queue database cannot be negative"
            );
        }

        if (settings.queue.empty()) {
            throw std::invalid_argument(
                "Redis queue name cannot be empty"
            );
        }

        if (
            settings
                .visibility_timeout
                .count() <= 0
        ) {
            throw std::invalid_argument(
                "Redis queue visibility timeout must be greater than zero"
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
                "Unable to allocate Redis queue connection"
            );
        }

        if (context->err != 0) {
            const auto message =
                std::string{
                    context->errstr
                };

            disconnect();

            throw std::runtime_error(
                "Unable to connect to Redis queue: " +
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
                "Unable to configure Redis queue timeout: " +
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
        std::vector<std::string_view>
            values{
                arguments
            };

        return command(
            values
        );
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
                "Redis queue command failed: " +
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
                "Redis queue command failed: " +
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

        for (const auto& value : owned) {
            views.emplace_back(value);
        }

        return command(views);
    }

    [[nodiscard]]
    std::size_t count(
        std::string_view operation,
        const std::string& key
    ) {
        auto reply =
            command({
                operation,
                key
            });

        require_integer(
            *reply,
            operation
        );

        if (reply->integer < 0) {
            throw std::runtime_error(
                "Redis queue count cannot be negative"
            );
        }

        return static_cast<
            std::size_t
        >(reply->integer);
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
                "Redis " +
                std::string{operation} +
                " returned an unexpected reply"
            );
        }
    }

    [[nodiscard]]
    std::string base_key()
        const {
        return
            settings.prefix +
            settings.queue +
            ":";
    }

    [[nodiscard]]
    std::string jobs_key()
        const {
        return
            base_key() +
            "jobs";
    }

    [[nodiscard]]
    std::string ready_key()
        const {
        return
            base_key() +
            "ready";
    }

    [[nodiscard]]
    std::string reserved_key()
        const {
        return
            base_key() +
            "reserved";
    }

    [[nodiscard]]
    std::string leases_key()
        const {
        return
            base_key() +
            "leases";
    }

    [[nodiscard]]
    std::string delayed_key()
        const {
        return
            base_key() +
            "delayed";
    }

    [[nodiscard]]
    std::string failed_key()
        const {
        return
            base_key() +
            "failed";
    }

    redisContext* context{
        nullptr
    };

    std::mutex mutex;
};

RedisDriver::RedisDriver(
    RedisSettings settings
)
    : impl_(
        std::make_unique<Impl>(
            std::move(settings)
        )
      ) {}

RedisDriver::~RedisDriver() =
    default;

RedisDriver::RedisDriver(
    RedisDriver&&
) noexcept = default;

RedisDriver& RedisDriver::operator=(
    RedisDriver&&
) noexcept = default;

void RedisDriver::push(
    Envelope job
) {
    impl_->push(
        std::move(job)
    );
}

void RedisDriver::push_later(
    Envelope job,
    std::chrono::milliseconds delay
) {
    impl_->push_later(
        std::move(job),
        delay
    );
}

std::optional<Envelope>
RedisDriver::pop() {
    return impl_->pop();
}

void RedisDriver::acknowledge(
    const Envelope& job
) {
    impl_->acknowledge(job);
}

void RedisDriver::release(
    Envelope job
) {
    impl_->release(
        std::move(job)
    );
}

void RedisDriver::release_after(
    Envelope job,
    std::chrono::milliseconds delay
) {
    impl_->release_after(
        std::move(job),
        delay
    );
}

bool RedisDriver::renew(
    const Envelope& job
) {
    return impl_->renew(job);
}

void RedisDriver::fail(
    const Envelope& job
) {
    impl_->fail(job);
}

bool RedisDriver::ping() {
    return impl_->ping();
}

std::size_t RedisDriver::pending() {
    return impl_->pending();
}

std::size_t RedisDriver::reserved() {
    return impl_->reserved();
}

std::size_t RedisDriver::delayed() {
    return impl_->delayed();
}

std::size_t RedisDriver::failed() {
    return impl_->failed();
}

void RedisDriver::flush() {
    impl_->flush();
}

const RedisSettings&
RedisDriver::settings()
    const noexcept {
    return impl_->settings;
}

} // namespace gungnir::queue
