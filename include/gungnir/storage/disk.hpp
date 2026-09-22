#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gungnir::storage {

class Disk {
public:
    virtual ~Disk() = default;

    [[nodiscard]] virtual bool exists(std::string_view path) const = 0;
    [[nodiscard]] virtual std::optional<std::string> get(std::string_view path) const = 0;
    virtual void put(std::string path, std::string contents) = 0;
    virtual bool remove(std::string_view path) = 0;
    virtual bool move(std::string_view from, std::string_view to) = 0;
    virtual bool copy(std::string_view from, std::string_view to) = 0;
    [[nodiscard]] virtual std::uintmax_t size(std::string_view path) const = 0;
    [[nodiscard]] virtual std::vector<std::string> files(std::string_view directory = {}) const = 0;
};

} // namespace gungnir::storage
