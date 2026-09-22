#pragma once

#include <filesystem>
#include <string_view>

#include <gungnir/core/types.hpp>

namespace gungnir::cli {

class Project {
public:
    [[nodiscard]] static Project create(
        std::filesystem::path destination,
        String name
    );

    [[nodiscard]] static Project open(
        std::filesystem::path start = {}
    );

    explicit Project(
        std::filesystem::path root
    );

    [[nodiscard]] const std::filesystem::path& root()
        const noexcept;

    [[nodiscard]] String name() const;

    [[nodiscard]] std::filesystem::path make_model(
        String name
    );

    [[nodiscard]] std::filesystem::path make_controller(
        String name
    );

    [[nodiscard]] std::filesystem::path make_middleware(
        String name
    );

    [[nodiscard]] std::filesystem::path make_migration(
        String name
    );

    [[nodiscard]] std::filesystem::path make_request(
        String name
    );

    [[nodiscard]] std::filesystem::path make_job(
        String name
    );

    [[nodiscard]] std::filesystem::path assemble() const;
    [[nodiscard]] std::filesystem::path assemble_migrations() const;

    int build(bool release = false) const;
    int run(bool release = false) const;
    int migrate(
        String command = "migrate",
        bool release = false
    ) const;

private:
    std::filesystem::path root_;

    [[nodiscard]] static String normalize_class_name(
        std::string_view value
    );

    [[nodiscard]] static String snake_case(
        std::string_view value
    );

    [[nodiscard]] static String migration_table(
        std::string_view value
    );

    [[nodiscard]] std::filesystem::path create_source(
        const std::filesystem::path& directory,
        const String& file_name,
        const String& content
    ) const;
};

} // namespace gungnir::cli
