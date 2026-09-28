#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include <gungnir/mail/transport.hpp>

namespace gungnir::mail {

enum class SmtpSecurity {
    none,
    start_tls,
    implicit_tls
};

struct SmtpSettings {
    std::string host{"127.0.0.1"};
    std::uint16_t port{587};
    SmtpSecurity security{
        SmtpSecurity::start_tls
    };
    std::optional<std::string> username;
    std::optional<std::string> password;
    std::chrono::milliseconds connect_timeout{
        5000
    };
    std::chrono::milliseconds transfer_timeout{
        30000
    };
    bool verify_peer{true};
    bool verify_host{true};
};

class SmtpTransport final :
    public Transport {
public:
    explicit SmtpTransport(
        SmtpSettings settings = {}
    );

    ~SmtpTransport() override;

    SmtpTransport(
        const SmtpTransport&
    ) = delete;

    SmtpTransport& operator=(
        const SmtpTransport&
    ) = delete;

    SmtpTransport(
        SmtpTransport&&
    ) noexcept;

    SmtpTransport& operator=(
        SmtpTransport&&
    ) noexcept;

    void send(
        const Message& message
    ) override;

    [[nodiscard]]
    const SmtpSettings& settings()
        const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gungnir::mail
