#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <gungnir/scheduler/timezone.hpp>

namespace gungnir::scheduler {

class IanaTimeZone final :
    public TimeZone {
public:
    explicit IanaTimeZone(
        std::string name
    );

    IanaTimeZone(
        std::string name,
        std::filesystem::path
            zoneinfo_root
    );

    [[nodiscard]]
    const std::string& name()
        const noexcept override;

    [[nodiscard]]
    LocalDateTime to_local(
        Clock::TimePoint point
    ) const override;

    [[nodiscard]]
    const std::filesystem::path&
    source()
        const noexcept;

private:
    struct Type {
        std::int32_t offset_seconds{0};
        bool daylight{false};
    };

    struct Transition {
        std::int64_t unix_seconds{0};
        std::uint8_t type{0};
    };

    void load(
        const std::filesystem::path&
            path
    );

    [[nodiscard]]
    std::size_t type_at(
        std::int64_t unix_seconds
    ) const noexcept;

    std::string name_;
    std::filesystem::path source_;
    std::vector<Type> types_;
    std::vector<Transition>
        transitions_;
    std::size_t default_type_{0};
};

[[nodiscard]]
std::filesystem::path
system_zoneinfo_root();

} // namespace gungnir::scheduler
