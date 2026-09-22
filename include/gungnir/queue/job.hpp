#pragma once

#include <string>
#include <string_view>

namespace gungnir::queue {

class Job {
public:
    virtual ~Job() = default;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] virtual std::string payload() const = 0;
};

struct Envelope {
    std::string id;
    std::string name;
    std::string payload;
    unsigned attempts{0};
    unsigned max_attempts{1};
};

} // namespace gungnir::queue
