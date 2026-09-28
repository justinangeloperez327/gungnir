#include <gungnir/redis/client.hpp>

#include <atomic>
#include <charconv>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>

#include <hiredis/hiredis.h>

#ifdef GUNGNIR_HIREDIS_SSL
#include <hiredis/hiredis_ssl.h>
#endif

namespace gungnir::redis {

namespace {

using NativeReply =
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
        static_cast<
            decltype(value.tv_sec)
        >(
            seconds.count()
        );

    value.tv_usec =
        static_cast<
            decltype(value.tv_usec)
        >(
            std::chrono::
                duration_cast<
                    std::chrono::
                        microseconds
                >(remainder)
                .count()
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

[[nodiscard]]
Reply convert_reply(
    const redisReply& reply
) {
    Reply result;

    switch (reply.type) {
    case REDIS_REPLY_NIL:
        result.type =
            ReplyType::nil;
        break;

    case REDIS_REPLY_STRING:
        result.type =
            ReplyType::string;
        result.text =
            reply_text(reply);
        break;

    case REDIS_REPLY_STATUS:
        result.type =
            ReplyType::status;
        result.text =
            reply_text(reply);
        break;

    case REDIS_REPLY_INTEGER:
        result.type =
            ReplyType::integer;
        result.integer =
            reply.integer;
        break;

    case REDIS_REPLY_ARRAY:
        result.type =
            ReplyType::array;
        result.elements.reserve(
            reply.elements
        );

        for (
            std::size_t index = 0;
            index < reply.elements;
            ++index
        ) {
            if (
                reply.element[index] ==
                nullptr
            ) {
                result.elements
                    .push_back({});
            } else {
                result.elements
                    .push_back(
                        convert_reply(
                            *reply.element[
                                index
                            ]
                        )
                    );
            }
        }
        break;

    case REDIS_REPLY_ERROR:
        throw std::runtime_error(
            "Redis command failed: " +
            reply_text(reply)
        );

    default:
        throw std::runtime_error(
            "Redis returned an unsupported reply type"
        );
    }

    return result;
}

[[nodiscard]]
Endpoint parse_redirect(
    std::string_view error
) {
    const auto first_space =
        error.find(' ');

    if (
        first_space ==
        std::string_view::npos
    ) {
        throw std::runtime_error(
            "Invalid Redis cluster redirect"
        );
    }

    const auto second_space =
        error.find(
            ' ',
            first_space + 1
        );

    if (
        second_space ==
        std::string_view::npos
    ) {
        throw std::runtime_error(
            "Invalid Redis cluster redirect"
        );
    }

    auto endpoint =
        error.substr(
            second_space + 1
        );

    const auto colon =
        endpoint.rfind(':');

    if (
        colon ==
        std::string_view::npos
    ) {
        throw std::runtime_error(
            "Invalid Redis cluster redirect endpoint"
        );
    }

    Endpoint result;
    result.host =
        std::string{
            endpoint.substr(
                0,
                colon
            )
        };

    const auto port_text =
        endpoint.substr(
            colon + 1
        );

    unsigned port = 0;

    const auto parsed =
        std::from_chars(
            port_text.data(),
            port_text.data() +
                port_text.size(),
            port
        );

    if (
        parsed.ec != std::errc{} ||
        port == 0 ||
        port > 65535
    ) {
        throw std::runtime_error(
            "Invalid Redis cluster redirect port"
        );
    }

    result.port =
        static_cast<std::uint16_t>(
            port
        );

    return result;
}

} // namespace

class Client::Impl {
public:
    struct Slot {
        redisContext* context{
            nullptr
        };

        Endpoint endpoint;
        std::mutex mutex;

#ifdef GUNGNIR_HIREDIS_SSL
        redisSSLContext*
            ssl_context{
                nullptr
            };
#endif

        ~Slot() {
            disconnect();
        }

        void disconnect()
            noexcept {
            if (context != nullptr) {
                redisFree(context);
                context = nullptr;
            }

#ifdef GUNGNIR_HIREDIS_SSL
            if (
                ssl_context !=
                nullptr
            ) {
                redisFreeSSLContext(
                    ssl_context
                );

                ssl_context =
                    nullptr;
            }
#endif
        }
    };

    explicit Impl(
        ClientOptions value
    )
        : options(
            std::move(value)
          ) {
        validate();

#ifdef GUNGNIR_HIREDIS_SSL
        if (options.tls.enabled) {
            redisInitOpenSSL();
        }
#endif

        slots.reserve(
            options.pool_size
        );

        for (
            std::size_t index = 0;
            index <
                options.pool_size;
            ++index
        ) {
            slots.push_back(
                std::make_unique<
                    Slot
                >()
            );
        }
    }

    void validate() {
        if (options.nodes.empty()) {
            throw std::invalid_argument(
                "Redis topology requires at least one node"
            );
        }

        for (
            const auto& node :
            options.nodes
        ) {
            if (
                node.host.empty() ||
                node.port == 0
            ) {
                throw std::invalid_argument(
                    "Redis nodes require a host and port"
                );
            }
        }

        if (
            options.pool_size == 0 ||
            options.redirect_limit == 0
        ) {
            throw std::invalid_argument(
                "Redis pool size and redirect limit must be greater than zero"
            );
        }

        if (
            options.database < 0
        ) {
            throw std::invalid_argument(
                "Redis database cannot be negative"
            );
        }

        if (
            options.topology ==
                Topology::cluster &&
            options.database != 0
        ) {
            throw std::invalid_argument(
                "Redis Cluster supports database 0 only"
            );
        }

        if (
            options.topology ==
                Topology::sentinel &&
            options
                .sentinel_master
                .empty()
        ) {
            throw std::invalid_argument(
                "Redis Sentinel requires a master name"
            );
        }

#ifndef GUNGNIR_HIREDIS_SSL
        if (options.tls.enabled) {
            throw std::logic_error(
                "Redis TLS requires hiredis SSL support at build time"
            );
        }
#endif

        static_cast<void>(
            timeout_value(
                options.connect_timeout
            )
        );

        static_cast<void>(
            timeout_value(
                options.command_timeout
            )
        );
    }

    [[nodiscard]]
    Endpoint resolve_sentinel() {
        std::exception_ptr
            last_error;

        for (
            const auto& sentinel :
            options.nodes
        ) {
            try {
                redisContext* context =
                    connect_raw(
                        sentinel,
                        false,
                        nullptr
                    );

                std::unique_ptr<
                    redisContext,
                    decltype(&redisFree)
                > guard{
                    context,
                    &redisFree
                };

                const std::vector<
                    std::string
                > command{
                    "SENTINEL",
                    "get-master-addr-by-name",
                    options
                        .sentinel_master
                };

                auto reply =
                    command_raw(
                        context,
                        command
                    );

                if (
                    reply->type !=
                        REDIS_REPLY_ARRAY ||
                    reply->elements != 2
                ) {
                    throw std::runtime_error(
                        "Redis Sentinel returned an invalid master endpoint"
                    );
                }

                const auto host =
                    reply_text(
                        *reply->element[0]
                    );

                const auto port_text =
                    reply_text(
                        *reply->element[1]
                    );

                unsigned port = 0;

                const auto parsed =
                    std::from_chars(
                        port_text.data(),
                        port_text.data() +
                            port_text.size(),
                        port
                    );

                if (
                    parsed.ec !=
                        std::errc{} ||
                    port == 0 ||
                    port > 65535
                ) {
                    throw std::runtime_error(
                        "Redis Sentinel returned an invalid master port"
                    );
                }

                return {
                    host,
                    static_cast<
                        std::uint16_t
                    >(port)
                };
            } catch (...) {
                last_error =
                    std::current_exception();
            }
        }

        if (last_error) {
            std::rethrow_exception(
                last_error
            );
        }

        throw std::runtime_error(
            "Unable to resolve Redis Sentinel master"
        );
    }

    [[nodiscard]]
    Endpoint target_endpoint(
        std::size_t slot_index
    ) {
        if (
            options.topology ==
            Topology::sentinel
        ) {
            return resolve_sentinel();
        }

        return
            options.nodes[
                slot_index %
                options.nodes.size()
            ];
    }

    [[nodiscard]]
    redisContext* connect_raw(
        const Endpoint& endpoint,
        bool authenticate,
        Slot* slot
    ) {
        auto timeout =
            timeout_value(
                options.connect_timeout
            );

        auto* context =
            redisConnectWithTimeout(
                endpoint.host.c_str(),
                static_cast<int>(
                    endpoint.port
                ),
                timeout
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

            redisFree(context);

            throw std::runtime_error(
                "Unable to connect to Redis: " +
                message
            );
        }

        try {
#ifdef GUNGNIR_HIREDIS_SSL
            if (
                options.tls.enabled &&
                slot != nullptr
            ) {
                redisSSLContextError
                    ssl_error;

                slot->ssl_context =
                    redisCreateSSLContext(
                        options.tls
                            .ca_file
                            .empty()
                            ? nullptr
                            : options.tls
                                .ca_file
                                .c_str(),
                        options.tls
                            .ca_path
                            .empty()
                            ? nullptr
                            : options.tls
                                .ca_path
                                .c_str(),
                        options.tls
                            .certificate
                            .empty()
                            ? nullptr
                            : options.tls
                                .certificate
                                .c_str(),
                        options.tls
                            .private_key
                            .empty()
                            ? nullptr
                            : options.tls
                                .private_key
                                .c_str(),
                        options.tls
                            .server_name
                            .empty()
                            ? endpoint
                                .host
                                .c_str()
                            : options.tls
                                .server_name
                                .c_str(),
                        &ssl_error
                    );

                if (
                    slot->ssl_context ==
                    nullptr
                ) {
                    throw std::runtime_error(
                        "Unable to create Redis TLS context"
                    );
                }

                if (
                    redisInitiateSSLWithContext(
                        context,
                        slot->ssl_context
                    ) != REDIS_OK
                ) {
                    throw std::runtime_error(
                        "Unable to negotiate Redis TLS"
                    );
                }
            }
#endif

            auto command_timeout =
                timeout_value(
                    options.command_timeout
                );

            if (
                redisSetTimeout(
                    context,
                    command_timeout
                ) != REDIS_OK
            ) {
                throw std::runtime_error(
                    "Unable to configure Redis command timeout"
                );
            }

            if (authenticate) {
                authenticate_context(
                    context
                );

                select_database(
                    context
                );
            }
        } catch (...) {
            redisFree(context);
            throw;
        }

        return context;
    }

    void authenticate_context(
        redisContext* context
    ) {
        if (!options.password) {
            return;
        }

        std::vector<std::string>
            command{"AUTH"};

        if (
            options.username &&
            !options
                .username
                ->empty()
        ) {
            command.push_back(
                *options.username
            );
        }

        command.push_back(
            *options.password
        );

        auto reply =
            command_raw(
                context,
                command
            );

        if (
            reply->type !=
                REDIS_REPLY_STATUS ||
            reply_text(*reply) !=
                "OK"
        ) {
            throw std::runtime_error(
                "Redis AUTH failed"
            );
        }
    }

    void select_database(
        redisContext* context
    ) {
        if (
            options.database == 0 ||
            options.topology ==
                Topology::cluster
        ) {
            return;
        }

        auto reply =
            command_raw(
                context,
                {
                    "SELECT",
                    std::to_string(
                        options.database
                    )
                }
            );

        if (
            reply->type !=
                REDIS_REPLY_STATUS ||
            reply_text(*reply) !=
                "OK"
        ) {
            throw std::runtime_error(
                "Redis SELECT failed"
            );
        }
    }

    [[nodiscard]]
    NativeReply command_raw(
        redisContext* context,
        const std::vector<
            std::string
        >& arguments
    ) {
        if (arguments.empty()) {
            throw std::invalid_argument(
                "Redis command must not be empty"
            );
        }

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
            const auto& argument :
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
            static_cast<
                redisReply*
            >(
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
            throw std::runtime_error(
                "Redis command returned no reply"
            );
        }

        return {
            raw,
            &freeReplyObject
        };
    }

    void connect_slot(
        Slot& slot,
        std::size_t index,
        std::optional<Endpoint>
            forced = std::nullopt
    ) {
        slot.disconnect();

        slot.endpoint =
            forced
                ? *forced
                : target_endpoint(
                    index
                  );

        slot.context =
            connect_raw(
                slot.endpoint,
                true,
                &slot
            );
    }

    [[nodiscard]]
    Reply execute(
        Slot& slot,
        std::size_t index,
        const std::vector<
            std::string
        >& arguments
    ) {
        std::optional<Endpoint>
            redirected;

        for (
            std::size_t attempt = 0;
            attempt <
                options.redirect_limit;
            ++attempt
        ) {
            if (
                slot.context ==
                    nullptr ||
                slot.context->err != 0
            ) {
                connect_slot(
                    slot,
                    index,
                    redirected
                );
            }

            auto reply =
                command_raw(
                    slot.context,
                    arguments
                );

            if (
                reply->type ==
                    REDIS_REPLY_ERROR &&
                options.topology ==
                    Topology::cluster
            ) {
                const auto error =
                    reply_text(*reply);

                if (
                    error.starts_with(
                        "MOVED "
                    ) ||
                    error.starts_with(
                        "ASK "
                    )
                ) {
                    redirected =
                        parse_redirect(
                            error
                        );

                    connect_slot(
                        slot,
                        index,
                        redirected
                    );

                    if (
                        error.starts_with(
                            "ASK "
                        )
                    ) {
                        auto asking =
                            command_raw(
                                slot.context,
                                {
                                    "ASKING"
                                }
                            );

                        if (
                            asking->type !=
                                REDIS_REPLY_STATUS
                        ) {
                            throw std::runtime_error(
                                "Redis ASKING failed"
                            );
                        }
                    }

                    continue;
                }
            }

            return
                convert_reply(
                    *reply
                );
        }

        throw std::runtime_error(
            "Redis cluster redirect limit exceeded"
        );
    }

    ClientOptions options;
    std::vector<
        std::unique_ptr<Slot>
    > slots;
    std::atomic_size_t next{0};
};

Client::Client(
    ClientOptions options
)
    : impl_(
        std::make_unique<Impl>(
            std::move(options)
        )
      ) {}

Client::~Client() = default;

Client::Client(
    Client&&
) noexcept = default;

Client& Client::operator=(
    Client&&
) noexcept = default;

Reply Client::command(
    const std::vector<
        std::string
    >& arguments
) {
    const auto index =
        impl_->next
            .fetch_add(
                1,
                std::memory_order_relaxed
            ) %
        impl_->slots.size();

    auto& slot =
        *impl_->slots[index];

    std::lock_guard lock{
        slot.mutex
    };

    try {
        return
            impl_->execute(
                slot,
                index,
                arguments
            );
    } catch (...) {
        slot.disconnect();
        throw;
    }
}

bool Client::ping() {
    try {
        return
            command({
                "PING"
            }).ok("PONG");
    } catch (...) {
        return false;
    }
}

void Client::reconnect() {
    for (
        auto& slot :
        impl_->slots
    ) {
        std::lock_guard lock{
            slot->mutex
        };

        slot->disconnect();
    }
}

const ClientOptions&
Client::options()
    const noexcept {
    return impl_->options;
}

Endpoint Client::active_endpoint()
    const {
    for (
        const auto& slot :
        impl_->slots
    ) {
        std::lock_guard lock{
            slot->mutex
        };

        if (
            slot->context !=
            nullptr
        ) {
            return slot->endpoint;
        }
    }

    if (
        impl_->options.topology ==
        Topology::sentinel
    ) {
        return
            const_cast<Impl*>(
                impl_.get()
            )->resolve_sentinel();
    }

    return
        impl_->options
            .nodes.front();
}

} // namespace gungnir::redis
