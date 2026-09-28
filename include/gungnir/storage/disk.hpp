#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gungnir/core/cancellation.hpp>

namespace gungnir::storage {

class Disk {
public:
    virtual ~Disk() = default;

    [[nodiscard]]
    virtual bool exists(
        std::string_view path
    ) const = 0;

    [[nodiscard]]
    virtual std::optional<std::string>
    get(
        std::string_view path
    ) const = 0;

    virtual void put(
        std::string path,
        std::string contents
    ) = 0;

    virtual bool remove(
        std::string_view path
    ) = 0;

    virtual bool move(
        std::string_view from,
        std::string_view to
    ) = 0;

    virtual bool copy(
        std::string_view from,
        std::string_view to
    ) = 0;

    [[nodiscard]]
    virtual std::uintmax_t size(
        std::string_view path
    ) const = 0;

    [[nodiscard]]
    virtual std::vector<std::string>
    files(
        std::string_view directory = {}
    ) const = 0;

    [[nodiscard]]
    virtual bool exists(
        std::string_view path,
        const CancellationToken& cancellation
    ) const {
        cancellation.throw_if_cancelled();

        const auto result =
            exists(path);

        cancellation.throw_if_cancelled();

        return result;
    }

    [[nodiscard]]
    virtual std::optional<std::string>
    get(
        std::string_view path,
        const CancellationToken& cancellation
    ) const {
        cancellation.throw_if_cancelled();

        auto result =
            get(path);

        cancellation.throw_if_cancelled();

        return result;
    }

    virtual void put(
        std::string path,
        std::string contents,
        const CancellationToken& cancellation
    ) {
        cancellation.throw_if_cancelled();

        put(
            std::move(path),
            std::move(contents)
        );

        cancellation.throw_if_cancelled();
    }

    virtual bool remove(
        std::string_view path,
        const CancellationToken& cancellation
    ) {
        cancellation.throw_if_cancelled();

        const auto result =
            remove(path);

        cancellation.throw_if_cancelled();

        return result;
    }

    virtual bool move(
        std::string_view from,
        std::string_view to,
        const CancellationToken& cancellation
    ) {
        cancellation.throw_if_cancelled();

        const auto result =
            move(from, to);

        cancellation.throw_if_cancelled();

        return result;
    }

    virtual bool copy(
        std::string_view from,
        std::string_view to,
        const CancellationToken& cancellation
    ) {
        cancellation.throw_if_cancelled();

        const auto result =
            copy(from, to);

        cancellation.throw_if_cancelled();

        return result;
    }

    [[nodiscard]]
    virtual std::uintmax_t size(
        std::string_view path,
        const CancellationToken& cancellation
    ) const {
        cancellation.throw_if_cancelled();

        const auto result =
            size(path);

        cancellation.throw_if_cancelled();

        return result;
    }

    [[nodiscard]]
    virtual std::vector<std::string>
    files(
        std::string_view directory,
        const CancellationToken& cancellation
    ) const {
        cancellation.throw_if_cancelled();

        auto result =
            files(directory);

        cancellation.throw_if_cancelled();

        return result;
    }
};

} // namespace gungnir::storage
