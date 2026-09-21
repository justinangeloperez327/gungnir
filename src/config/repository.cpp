#include <gungnir/config/repository.hpp>

#include <charconv>
#include <stdexcept>
#include <utility>

namespace gungnir::config {

Repository& Repository::set(
    String key,
    Value value
) {
    values_.insert_or_assign(
        std::move(key),
        std::move(value)
    );

    return *this;
}

Repository& Repository::set(
    String key,
    String value
) {
    return set(
        std::move(key),
        Value{std::move(value)}
    );
}

Repository& Repository::set(
    String key,
    const char* value
) {
    return set(
        std::move(key),
        String{value ? value : ""}
    );
}

Repository& Repository::set(
    String key,
    Boolean value
) {
    return set(
        std::move(key),
        Value{value}
    );
}

Repository& Repository::set(
    String key,
    Int64 value
) {
    return set(
        std::move(key),
        Value{value}
    );
}

Repository& Repository::set(
    String key,
    Double value
) {
    return set(
        std::move(key),
        Value{value}
    );
}

bool Repository::has(
    std::string_view key
) const {
    return
        values_.find(String{key}) !=
        values_.end();
}

std::optional<Value> Repository::find(
    std::string_view key
) const {
    const auto found =
        values_.find(String{key});

    if (found == values_.end()) {
        return std::nullopt;
    }

    return found->second;
}

String Repository::string(
    std::string_view key,
    String fallback
) const {
    const auto value = find(key);

    if (!value) {
        return fallback;
    }

    return std::visit(
        [&fallback](const auto& current) -> String {
            using Current =
                std::remove_cvref_t<
                    decltype(current)
                >;

            if constexpr (
                std::same_as<
                    Current,
                    std::monostate
                >
            ) {
                return fallback;
            } else if constexpr (
                std::same_as<
                    Current,
                    String
                >
            ) {
                return current;
            } else if constexpr (
                std::same_as<
                    Current,
                    Boolean
                >
            ) {
                return current
                    ? "true"
                    : "false";
            } else {
                return std::to_string(current);
            }
        },
        *value
    );
}

Int64 Repository::integer(
    std::string_view key,
    Int64 fallback
) const {
    const auto value = find(key);

    if (!value) {
        return fallback;
    }

    if (
        const auto* integer =
            std::get_if<Int64>(&*value)
    ) {
        return *integer;
    }

    if (
        const auto* boolean =
            std::get_if<Boolean>(&*value)
    ) {
        return *boolean ? 1 : 0;
    }

    if (
        const auto* number =
            std::get_if<Double>(&*value)
    ) {
        return static_cast<Int64>(*number);
    }

    if (
        const auto* text =
            std::get_if<String>(&*value)
    ) {
        Int64 parsed = 0;

        const auto result = std::from_chars(
            text->data(),
            text->data() + text->size(),
            parsed
        );

        if (
            result.ec == std::errc{} &&
            result.ptr ==
                text->data() + text->size()
        ) {
            return parsed;
        }
    }

    throw std::invalid_argument(
        "Configuration value '" +
        String{key} +
        "' is not an integer"
    );
}

Boolean Repository::boolean(
    std::string_view key,
    Boolean fallback
) const {
    const auto value = find(key);

    if (!value) {
        return fallback;
    }

    if (
        const auto* boolean =
            std::get_if<Boolean>(&*value)
    ) {
        return *boolean;
    }

    if (
        const auto* integer =
            std::get_if<Int64>(&*value)
    ) {
        return *integer != 0;
    }

    if (
        const auto* text =
            std::get_if<String>(&*value)
    ) {
        if (
            *text == "true" ||
            *text == "1" ||
            *text == "yes" ||
            *text == "on"
        ) {
            return true;
        }

        if (
            *text == "false" ||
            *text == "0" ||
            *text == "no" ||
            *text == "off"
        ) {
            return false;
        }
    }

    throw std::invalid_argument(
        "Configuration value '" +
        String{key} +
        "' is not boolean"
    );
}

Double Repository::number(
    std::string_view key,
    Double fallback
) const {
    const auto value = find(key);

    if (!value) {
        return fallback;
    }

    if (
        const auto* number =
            std::get_if<Double>(&*value)
    ) {
        return *number;
    }

    if (
        const auto* integer =
            std::get_if<Int64>(&*value)
    ) {
        return static_cast<Double>(*integer);
    }

    if (
        const auto* text =
            std::get_if<String>(&*value)
    ) {
        Double parsed = 0.0;

        const auto result = std::from_chars(
            text->data(),
            text->data() + text->size(),
            parsed
        );

        if (
            result.ec == std::errc{} &&
            result.ptr ==
                text->data() + text->size()
        ) {
            return parsed;
        }
    }

    throw std::invalid_argument(
        "Configuration value '" +
        String{key} +
        "' is not numeric"
    );
}

} // namespace gungnir::config
