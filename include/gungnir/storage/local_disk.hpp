#pragma once

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include <gungnir/storage/disk.hpp>
#include <gungnir/storage/error.hpp>

namespace gungnir::storage {

class LocalDisk final : public Disk {
public:
    explicit LocalDisk(std::filesystem::path root) : root_(std::move(root)) {}

    [[nodiscard]] const std::filesystem::path& root() const noexcept { return root_; }

    [[nodiscard]] bool exists(std::string_view path) const override {
        return std::filesystem::is_regular_file(resolve(path));
    }

    [[nodiscard]] std::optional<std::string> get(std::string_view path) const override {
        const auto target = resolve(path);
        std::ifstream input{target, std::ios::binary};
        if (!input) return std::nullopt;
        return std::string{
            std::istreambuf_iterator<char>{input},
            std::istreambuf_iterator<char>{}
        };
    }

    void put(std::string path, std::string contents) override {
        const auto target = resolve(path);
        if (const auto parent = target.parent_path(); !parent.empty()) {
            std::filesystem::create_directories(parent);
        }
        std::ofstream output{target, std::ios::binary | std::ios::trunc};
        if (!output) throw Error{"Unable to write storage object: " + path};
        output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        if (!output) throw Error{"Unable to write storage object: " + path};
    }

    bool remove(std::string_view path) override {
        return std::filesystem::remove(resolve(path));
    }

    bool move(std::string_view from, std::string_view to) override {
        const auto source = resolve(from);
        const auto destination = resolve(to);
        if (!std::filesystem::exists(source)) return false;
        if (const auto parent = destination.parent_path(); !parent.empty()) {
            std::filesystem::create_directories(parent);
        }
        std::filesystem::rename(source, destination);
        return true;
    }

    bool copy(std::string_view from, std::string_view to) override {
        const auto source = resolve(from);
        const auto destination = resolve(to);
        if (!std::filesystem::exists(source)) return false;
        if (const auto parent = destination.parent_path(); !parent.empty()) {
            std::filesystem::create_directories(parent);
        }
        return std::filesystem::copy_file(
            source,
            destination,
            std::filesystem::copy_options::overwrite_existing
        );
    }

    [[nodiscard]] std::uintmax_t size(std::string_view path) const override {
        const auto target = resolve(path);
        if (!std::filesystem::is_regular_file(target)) throw NotFound{std::string{path}};
        return std::filesystem::file_size(target);
    }

    [[nodiscard]] std::vector<std::string> files(std::string_view directory = {}) const override {
        const auto target = resolve(directory);
        std::vector<std::string> result;
        if (!std::filesystem::exists(target)) return result;
        if (!std::filesystem::is_directory(target)) return result;
        for (const auto& entry : std::filesystem::directory_iterator{target}) {
            if (entry.is_regular_file()) {
                result.push_back(
                    std::filesystem::relative(entry.path(), root_).generic_string()
                );
            }
        }
        return result;
    }

private:
    [[nodiscard]] std::filesystem::path resolve(std::string_view value) const {
        const std::filesystem::path relative{value};
        if (relative.is_absolute()) throw InvalidPath{std::string{value}};
        for (const auto& part : relative) {
            if (part == "..") throw InvalidPath{std::string{value}};
        }
        return root_ / relative;
    }

    std::filesystem::path root_;
};

} // namespace gungnir::storage
