#include <gungnir/session/redis_store.hpp>

#include <charconv>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace gungnir::session {

namespace {

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
            "Invalid Redis session payload"
        );
    }

    std::size_t length = 0;

    const auto parsed =
        std::from_chars(
            input.data() + cursor,
            input.data() + separator,
            length
        );

    if (
        parsed.ec !=
            std::errc{} ||
        parsed.ptr !=
            input.data() + separator
    ) {
        throw std::runtime_error(
            "Invalid Redis session field length"
        );
    }

    cursor =
        separator + 1;

    if (
        length >
        input.size() - cursor
    ) {
        throw std::runtime_error(
            "Truncated Redis session payload"
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
std::size_t read_count(
    std::string_view input,
    std::size_t& cursor
) {
    const auto value =
        read_field(
            input,
            cursor
        );

    std::size_t count = 0;

    const auto parsed =
        std::from_chars(
            value.data(),
            value.data() +
                value.size(),
            count
        );

    if (
        parsed.ec !=
            std::errc{} ||
        parsed.ptr !=
            value.data() +
                value.size()
    ) {
        throw std::runtime_error(
            "Invalid Redis session map count"
        );
    }

    return count;
}

void append_map(
    std::string& output,
    const std::unordered_map<
        std::string,
        std::string
    >& values
) {
    const auto count =
        std::to_string(
            values.size()
        );

    append_field(
        output,
        count
    );

    for (
        const auto& [key, value] :
        values
    ) {
        append_field(
            output,
            key
        );

        append_field(
            output,
            value
        );
    }
}

[[nodiscard]]
std::unordered_map<
    std::string,
    std::string
> read_map(
    std::string_view input,
    std::size_t& cursor
) {
    const auto count =
        read_count(
            input,
            cursor
        );

    std::unordered_map<
        std::string,
        std::string
    > values;

    values.reserve(count);

    for (
        std::size_t index = 0;
        index < count;
        ++index
    ) {
        auto key =
            std::string{
                read_field(
                    input,
                    cursor
                )
            };

        auto value =
            std::string{
                read_field(
                    input,
                    cursor
                )
            };

        values.insert_or_assign(
            std::move(key),
            std::move(value)
        );
    }

    return values;
}

[[nodiscard]]
std::string encode(
    const Session& session
) {
    const auto state =
        session.state();

    std::string output;

    append_field(
        output,
        "1"
    );

    append_field(
        output,
        state.id
    );

    append_map(
        output,
        state.values
    );

    append_map(
        output,
        state.flash_current
    );

    append_map(
        output,
        state.flash_next
    );

    return output;
}

[[nodiscard]]
Session decode(
    std::string_view encoded
) {
    std::size_t cursor = 0;

    const auto version =
        read_field(
            encoded,
            cursor
        );

    if (version != "1") {
        throw std::runtime_error(
            "Unsupported Redis session payload version"
        );
    }

    SessionState state;

    state.id =
        std::string{
            read_field(
                encoded,
                cursor
            )
        };

    state.values =
        read_map(
            encoded,
            cursor
        );

    state.flash_current =
        read_map(
            encoded,
            cursor
        );

    state.flash_next =
        read_map(
            encoded,
            cursor
        );

    if (
        cursor !=
        encoded.size()
    ) {
        throw std::runtime_error(
            "Redis session payload contains trailing data"
        );
    }

    return Session::restore(
        std::move(state)
    );
}

} // namespace

class RedisStore::Impl {
public:
    explicit Impl(
        RedisSessionSettings value
    )
        : settings(
            std::move(value)
          ),
          cache(
            settings.redis
          ) {
        if (
            settings.lifetime.count() <=
            0
        ) {
            throw std::invalid_argument(
                "Redis session lifetime must be greater than zero"
            );
        }

        if (
            settings.redis.prefix.empty()
        ) {
            throw std::invalid_argument(
                "Redis session prefix cannot be empty"
            );
        }
    }

    [[nodiscard]]
    std::optional<Session> load(
        std::string_view id
    ) {
        if (id.empty()) {
            return std::nullopt;
        }

        const auto payload =
            cache.get(id);

        if (!payload) {
            return std::nullopt;
        }

        auto session =
            decode(
                *payload
            );

        if (
            session.id() != id
        ) {
            throw std::runtime_error(
                "Redis session key does not match persisted session id"
            );
        }

        return session;
    }

    void save(
        const Session& session
    ) {
        if (session.id().empty()) {
            throw std::invalid_argument(
                "Redis session id cannot be empty"
            );
        }

        cache.put(
            std::string{
                session.id()
            },
            encode(session),
            settings.lifetime
        );
    }

    void erase(
        std::string_view id
    ) {
        if (id.empty()) {
            return;
        }

        static_cast<void>(
            cache.forget(id)
        );
    }

    [[nodiscard]]
    bool ping() {
        return cache.ping();
    }

    void flush() {
        cache.flush();
    }

    RedisSessionSettings settings;
    cache::RedisStore cache;
};

RedisStore::RedisStore(
    RedisSessionSettings settings
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

std::optional<Session>
RedisStore::load(
    std::string_view id
) {
    return impl_->load(id);
}

void RedisStore::save(
    const Session& session
) {
    impl_->save(session);
}

void RedisStore::erase(
    std::string_view id
) {
    impl_->erase(id);
}

bool RedisStore::ping() {
    return impl_->ping();
}

void RedisStore::flush() {
    impl_->flush();
}

const RedisSessionSettings&
RedisStore::settings()
    const noexcept {
    return impl_->settings;
}

} // namespace gungnir::session
