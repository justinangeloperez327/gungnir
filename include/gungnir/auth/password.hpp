#pragma once
#include <string>
#include <string_view>

namespace gungnir::auth {
class Password {
public:
    [[nodiscard]] static std::string hash(std::string_view password);
    [[nodiscard]] static bool verify(std::string_view password, std::string_view encoded);
    [[nodiscard]] static bool needs_rehash(std::string_view encoded) noexcept;
    [[nodiscard]] static std::string token_digest(std::string_view token);
};
}
