#pragma once
#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace gungnir::http {

class Transport {
public:
    virtual ~Transport() = default;
    [[nodiscard]] virtual std::size_t read(std::span<char> buffer) = 0;
    virtual void write(std::string_view data) = 0;
    virtual void close() noexcept = 0;
    [[nodiscard]] virtual bool secure() const noexcept { return false; }
};

class TlsContext {
public:
    virtual ~TlsContext() = default;
    [[nodiscard]] virtual std::string_view protocol() const noexcept = 0;
};

} // namespace gungnir::http
