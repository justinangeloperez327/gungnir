#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gungnir/storage/disk.hpp>

namespace gungnir::storage {

class LocalDisk final :
    public Disk {
public:
    explicit LocalDisk(
        std::filesystem::path root
    );

    [[nodiscard]]
    const std::filesystem::path&
    root() const noexcept;

    [[nodiscard]]
    bool exists(
        std::string_view path
    ) const override;

    [[nodiscard]]
    bool exists(
        std::string_view path,
        const CancellationToken& cancellation
    ) const override;

    [[nodiscard]]
    std::optional<std::string>
    get(
        std::string_view path
    ) const override;

    [[nodiscard]]
    std::optional<std::string>
    get(
        std::string_view path,
        const CancellationToken& cancellation
    ) const override;

    void put(
        std::string path,
        std::string contents
    ) override;

    void put(
        std::string path,
        std::string contents,
        const CancellationToken& cancellation
    ) override;

    bool remove(
        std::string_view path
    ) override;

    bool remove(
        std::string_view path,
        const CancellationToken& cancellation
    ) override;

    bool move(
        std::string_view from,
        std::string_view to
    ) override;

    bool move(
        std::string_view from,
        std::string_view to,
        const CancellationToken& cancellation
    ) override;

    bool copy(
        std::string_view from,
        std::string_view to
    ) override;

    bool copy(
        std::string_view from,
        std::string_view to,
        const CancellationToken& cancellation
    ) override;

    [[nodiscard]]
    std::uintmax_t size(
        std::string_view path
    ) const override;

    [[nodiscard]]
    std::uintmax_t size(
        std::string_view path,
        const CancellationToken& cancellation
    ) const override;

    [[nodiscard]]
    std::vector<std::string>
    files(
        std::string_view directory = {}
    ) const override;

    [[nodiscard]]
    std::vector<std::string>
    files(
        std::string_view directory,
        const CancellationToken& cancellation
    ) const override;

    [[nodiscard]]
    std::size_t cleanup_abandoned(
        std::chrono::seconds older_than =
            std::chrono::hours{24}
    );

private:
    [[nodiscard]]
    std::filesystem::path validate_relative(
        std::string_view value
    ) const;

    [[nodiscard]]
    std::filesystem::path resolve(
        std::string_view value
    ) const;

    [[nodiscard]]
    std::filesystem::path prepare_destination(
        std::string_view value
    ) const;

    void reject_symlinks(
        const std::filesystem::path& relative
    ) const;

    void atomic_put(
        const std::filesystem::path& target,
        std::string_view original_path,
        std::string_view contents,
        const CancellationToken& cancellation
    ) const;

    std::filesystem::path root_;
    std::uint64_t root_identity_a_{0};
    std::uint64_t root_identity_b_{0};
};

} // namespace gungnir::storage
