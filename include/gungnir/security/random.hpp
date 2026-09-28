#pragma once

#include <cstddef>
#include <string>

namespace gungnir::security {

[[nodiscard]]
std::string random_token(
    std::size_t bytes = 32
);

} // namespace gungnir::security
