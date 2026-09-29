#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gungnir::redis {

enum class Topology {
    standalone,
    sentinel,
    cluster
};

struct Endpoint {
    std::string host{"127.0.0.1"};
    std::uint16_t port{6379};
};

struct TlsOptions {
    bool enabled{false};
    std::string ca_file;
    std::string ca_path;
    std::string certificate;
    std::string private_key;
    std::string server_name;
};

struct ClientOptions {
    Topology topology{
        Topology::standalone
    };

    std::vector<Endpoint> nodes{
        Endpoint{}
    };

    std::string sentinel_master{
        "mymaster"
    };

    std::optional<std::string>
        username;

    std::optional<std::string>
        password;

    int database{0};

    std::size_t pool_size{4};

    std::chrono::milliseconds
        connect_timeout{2000};

    std::chrono::milliseconds
        command_timeout{2000};

    std::size_t redirect_limit{4};

    TlsOptions tls;
};

enum class ReplyType {
    nil,
    string,
    status,
    integer,
    array
};

struct Reply {
    ReplyType type{
        ReplyType::nil
    };
    std::string text;
    std::int64_t integer{0};
    std::vector<Reply> elements;

    [[nodiscard]]
    bool ok(
        std::string_view value = "OK"
    ) const noexcept {
        return
            type ==
                ReplyType::status &&
            text == value;
    }
};

class Client {
public:
    explicit Client(
        ClientOptions options = {}
    );

    ~Client();

    Client(
        const Client&
    ) = delete;

    Client& operator=(
        const Client&
    ) = delete;

    Client(
        Client&&
    ) noexcept;

    Client& operator=(
        Client&&
    ) noexcept;

    [[nodiscard]]
    Reply command(
        const std::vector<
            std::string
        >& arguments
    );

    [[nodiscard]]
    bool ping();

    void reconnect();

    [[nodiscard]]
    const ClientOptions& options()
        const noexcept;

    [[nodiscard]]
    Endpoint active_endpoint()
        const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gungnir::redis
