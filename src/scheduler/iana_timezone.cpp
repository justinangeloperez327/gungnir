#include <gungnir/scheduler/iana_timezone.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string_view>

namespace gungnir::scheduler {

namespace {

struct Header {
    char version{'\0'};
    std::uint32_t ttisgmtcnt{0};
    std::uint32_t ttisstdcnt{0};
    std::uint32_t leapcnt{0};
    std::uint32_t timecnt{0};
    std::uint32_t typecnt{0};
    std::uint32_t charcnt{0};
};

[[nodiscard]]
std::uint32_t read_u32(
    const std::vector<
        unsigned char
    >& bytes,
    std::size_t offset
) {
    if (
        offset + 4 >
        bytes.size()
    ) {
        throw std::runtime_error(
            "Truncated TZif file"
        );
    }

    return
        (
            static_cast<std::uint32_t>(
                bytes[offset]
            ) << 24
        ) |
        (
            static_cast<std::uint32_t>(
                bytes[offset + 1]
            ) << 16
        ) |
        (
            static_cast<std::uint32_t>(
                bytes[offset + 2]
            ) << 8
        ) |
        static_cast<std::uint32_t>(
            bytes[offset + 3]
        );
}

[[nodiscard]]
std::int32_t read_i32(
    const std::vector<
        unsigned char
    >& bytes,
    std::size_t offset
) {
    return
        static_cast<std::int32_t>(
            read_u32(
                bytes,
                offset
            )
        );
}

[[nodiscard]]
std::int64_t read_i64(
    const std::vector<
        unsigned char
    >& bytes,
    std::size_t offset
) {
    if (
        offset + 8 >
        bytes.size()
    ) {
        throw std::runtime_error(
            "Truncated TZif file"
        );
    }

    std::uint64_t value = 0;

    for (
        std::size_t index = 0;
        index < 8;
        ++index
    ) {
        value =
            (
                value << 8
            ) |
            bytes[
                offset + index
            ];
    }

    return
        static_cast<std::int64_t>(
            value
        );
}

[[nodiscard]]
Header read_header(
    const std::vector<
        unsigned char
    >& bytes,
    std::size_t offset
) {
    if (
        offset + 44 >
        bytes.size()
    ) {
        throw std::runtime_error(
            "Truncated TZif header"
        );
    }

    if (
        std::string_view{
            reinterpret_cast<
                const char*
            >(
                bytes.data() +
                offset
            ),
            4
        } != "TZif"
    ) {
        throw std::runtime_error(
            "Invalid TZif magic"
        );
    }

    Header header;

    header.version =
        static_cast<char>(
            bytes[offset + 4]
        );

    header.ttisgmtcnt =
        read_u32(
            bytes,
            offset + 20
        );

    header.ttisstdcnt =
        read_u32(
            bytes,
            offset + 24
        );

    header.leapcnt =
        read_u32(
            bytes,
            offset + 28
        );

    header.timecnt =
        read_u32(
            bytes,
            offset + 32
        );

    header.typecnt =
        read_u32(
            bytes,
            offset + 36
        );

    header.charcnt =
        read_u32(
            bytes,
            offset + 40
        );

    if (
        header.typecnt == 0 ||
        header.typecnt > 256
    ) {
        throw std::runtime_error(
            "TZif file has an invalid type table"
        );
    }

    return header;
}

[[nodiscard]]
std::size_t block_size(
    const Header& header,
    std::size_t time_size
) {
    return
        static_cast<std::size_t>(
            header.timecnt
        ) *
            time_size +
        header.timecnt +
        static_cast<std::size_t>(
            header.typecnt
        ) *
            6 +
        header.charcnt +
        static_cast<std::size_t>(
            header.leapcnt
        ) *
            (
                time_size + 4
            ) +
        header.ttisstdcnt +
        header.ttisgmtcnt;
}

void validate_zone_name(
    std::string_view name
) {
    if (
        name.empty() ||
        name.front() == '/' ||
        name.find("..") !=
            std::string_view::npos ||
        name.find('\\') !=
            std::string_view::npos
    ) {
        throw std::invalid_argument(
            "IANA timezone name must be a relative zone identifier"
        );
    }
}

} // namespace

std::filesystem::path
system_zoneinfo_root() {
    if (
        const auto* tzdir =
            std::getenv("TZDIR");
        tzdir != nullptr &&
        *tzdir != '\0'
    ) {
        return
            std::filesystem::path{
                tzdir
            };
    }

    constexpr std::array<
        std::string_view,
        4
    > candidates{
        "/usr/share/zoneinfo",
        "/usr/share/lib/zoneinfo",
        "/usr/lib/zoneinfo",
        "/etc/zoneinfo"
    };

    for (
        const auto candidate :
        candidates
    ) {
        std::error_code error;

        if (
            std::filesystem::
                is_directory(
                    candidate,
                    error
                ) &&
            !error
        ) {
            return
                std::filesystem::path{
                    candidate
                };
        }
    }

    throw std::runtime_error(
        "No IANA zoneinfo database was found; set TZDIR explicitly"
    );
}

IanaTimeZone::IanaTimeZone(
    std::string name
)
    : IanaTimeZone(
        std::move(name),
        system_zoneinfo_root()
      ) {}

IanaTimeZone::IanaTimeZone(
    std::string name,
    std::filesystem::path
        zoneinfo_root
)
    : name_(
        std::move(name)
      ) {
    validate_zone_name(name_);

    if (
        zoneinfo_root.empty()
    ) {
        throw std::invalid_argument(
            "IANA zoneinfo root must not be empty"
        );
    }

    const auto root =
        std::filesystem::
            weakly_canonical(
                zoneinfo_root
            );

    const auto candidate =
        std::filesystem::
            weakly_canonical(
                root /
                std::filesystem::path{
                    name_
                }
            );

    const auto relative =
        candidate.lexically_relative(
            root
        );

    const auto first_component =
        relative.empty()
            ? relative.end()
            : relative.begin();

    if (
        relative.empty() ||
        (
            first_component !=
                relative.end() &&
            *first_component ==
                std::filesystem::path{
                    ".."
                }
        )
    ) {
        throw std::invalid_argument(
            "IANA timezone resolves outside the zoneinfo root"
        );
    }

    source_ = candidate;
    load(candidate);
}

const std::string&
IanaTimeZone::name()
    const noexcept {
    return name_;
}

const std::filesystem::path&
IanaTimeZone::source()
    const noexcept {
    return source_;
}

void IanaTimeZone::load(
    const std::filesystem::path& path
) {
    std::ifstream input{
        path,
        std::ios::binary
    };

    if (!input) {
        throw std::runtime_error(
            "Unable to open IANA timezone '" +
            name_ +
            "'"
        );
    }

    std::vector<unsigned char>
        bytes{
            std::istreambuf_iterator<
                char
            >{input},
            std::istreambuf_iterator<
                char
            >{}
        };

    auto header =
        read_header(
            bytes,
            0
        );

    std::size_t data_offset = 44;
    std::size_t time_size = 4;

    if (
        header.version == '2' ||
        header.version == '3' ||
        header.version == '4'
    ) {
        const auto second_header =
            44 +
            block_size(
                header,
                4
            );

        header =
            read_header(
                bytes,
                second_header
            );

        data_offset =
            second_header + 44;

        time_size = 8;
    }

    const auto required =
        data_offset +
        block_size(
            header,
            time_size
        );

    if (required > bytes.size()) {
        throw std::runtime_error(
            "Truncated IANA timezone data"
        );
    }

    std::vector<std::int64_t>
        transition_times;

    transition_times.reserve(
        header.timecnt
    );

    auto cursor = data_offset;

    for (
        std::uint32_t index = 0;
        index < header.timecnt;
        ++index
    ) {
        transition_times.push_back(
            time_size == 8
                ? read_i64(
                    bytes,
                    cursor
                  )
                : read_i32(
                    bytes,
                    cursor
                  )
        );

        cursor += time_size;
    }

    std::vector<std::uint8_t>
        transition_types;

    transition_types.reserve(
        header.timecnt
    );

    for (
        std::uint32_t index = 0;
        index < header.timecnt;
        ++index
    ) {
        transition_types.push_back(
            bytes[cursor++]
        );
    }

    types_.clear();
    types_.reserve(
        header.typecnt
    );

    for (
        std::uint32_t index = 0;
        index < header.typecnt;
        ++index
    ) {
        const auto offset =
            read_i32(
                bytes,
                cursor
            );

        const auto daylight =
            bytes[
                cursor + 4
            ] != 0;

        const auto abbreviation =
            bytes[
                cursor + 5
            ];

        if (
            abbreviation >=
            header.charcnt &&
            header.charcnt != 0
        ) {
            throw std::runtime_error(
                "IANA timezone has an invalid abbreviation index"
            );
        }

        types_.push_back({
            offset,
            daylight
        });

        cursor += 6;
    }

    default_type_ = 0;

    for (
        std::size_t index = 0;
        index < types_.size();
        ++index
    ) {
        if (!types_[index].daylight) {
            default_type_ = index;
            break;
        }
    }

    transitions_.clear();
    transitions_.reserve(
        transition_times.size()
    );

    for (
        std::size_t index = 0;
        index <
            transition_times.size();
        ++index
    ) {
        const auto type =
            transition_types[
                index
            ];

        if (
            type >=
            types_.size()
        ) {
            throw std::runtime_error(
                "IANA timezone transition references an invalid type"
            );
        }

        transitions_.push_back({
            transition_times[index],
            type
        });
    }
}

std::size_t IanaTimeZone::type_at(
    std::int64_t unix_seconds
) const noexcept {
    const auto found =
        std::upper_bound(
            transitions_.begin(),
            transitions_.end(),
            unix_seconds,
            [](
                std::int64_t value,
                const Transition& item
            ) {
                return
                    value <
                    item.unix_seconds;
            }
        );

    if (
        found ==
        transitions_.begin()
    ) {
        return default_type_;
    }

    return
        static_cast<std::size_t>(
            std::prev(found)->type
        );
}

LocalDateTime
IanaTimeZone::to_local(
    Clock::TimePoint point
) const {
    const auto seconds =
        std::chrono::
            duration_cast<
                std::chrono::seconds
            >(
                point.time_since_epoch()
            ).count();

    const auto index =
        type_at(seconds);

    const auto& type =
        types_.at(index);

    return
        detail::local_from_offset(
            point,
            std::chrono::duration_cast<
                std::chrono::minutes
            >(
                std::chrono::seconds{
                    type.offset_seconds
                }
            ),
            type.daylight
        );
}

} // namespace gungnir::scheduler
