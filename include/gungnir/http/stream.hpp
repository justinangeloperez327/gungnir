#pragma once
#include <functional>
#include <optional>
#include <string>

namespace gungnir::http {

class BodyStream {
public:
    using Producer = std::function<std::optional<std::string>()>;
    explicit BodyStream(Producer producer) : producer_(std::move(producer)) {}
    [[nodiscard]] std::optional<std::string> next() { return producer_ ? producer_() : std::nullopt; }
private:
    Producer producer_;
};

} // namespace gungnir::http
