#pragma once
#include <string>
#include <string_view>
#include <utility>

namespace gungnir::extensions {

struct Package {
    std::string name;
    std::string version;
    std::string gungnir;

    [[nodiscard]] bool valid() const noexcept {
        return !name.empty() && !version.empty();
    }
};

} // namespace gungnir::extensions
