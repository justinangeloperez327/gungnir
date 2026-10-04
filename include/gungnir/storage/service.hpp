#pragma once

#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include <gungnir/core/types.hpp>
#include <gungnir/storage/manager.hpp>

namespace gungnir::storage {

class FileStore {
public:
    explicit FileStore(std::shared_ptr<Disk> disk) : disk_(std::move(disk)) {
        if (!disk_) throw std::invalid_argument("Storage disk is not configured");
    }

    [[nodiscard]] bool exists(std::string_view path) const { return disk_->exists(path); }
    [[nodiscard]] std::optional<String> get(std::string_view path) const { return disk_->get(path); }
    void put(String path, String contents) const { disk_->put(std::move(path), std::move(contents)); }
    bool remove(std::string_view path) const { return disk_->remove(path); }
    bool copy(std::string_view from, std::string_view to) const { return disk_->copy(from, to); }
    bool move(std::string_view from, std::string_view to) const { return disk_->move(from, to); }
    [[nodiscard]] UInt64 size(std::string_view path) const {
        const auto value = disk_->size(path);
        if (!std::in_range<UInt64>(value)) throw std::length_error("Storage file size is out of range");
        return static_cast<UInt64>(value);
    }
    [[nodiscard]] std::vector<String> files(std::string_view directory = {}) const { return disk_->files(directory); }

private:
    std::shared_ptr<Disk> disk_;
};

class Service {
public:
    explicit Service(std::shared_ptr<Manager> manager) : manager_(std::move(manager)) {
        if (!manager_) throw std::invalid_argument("Storage manager is not configured");
    }

    [[nodiscard]] FileStore disk(std::string_view name = {}) const { return FileStore{manager_->shared_disk(name)}; }
    [[nodiscard]] bool exists(std::string_view path) const { return disk().exists(path); }
    [[nodiscard]] std::optional<String> get(std::string_view path) const { return disk().get(path); }
    void put(String path, String contents) const { disk().put(std::move(path), std::move(contents)); }
    bool remove(std::string_view path) const { return disk().remove(path); }
    bool copy(std::string_view from, std::string_view to) const { return disk().copy(from, to); }
    bool move(std::string_view from, std::string_view to) const { return disk().move(from, to); }
    [[nodiscard]] UInt64 size(std::string_view path) const { return disk().size(path); }
    [[nodiscard]] std::vector<String> files(std::string_view directory = {}) const { return disk().files(directory); }

private:
    std::shared_ptr<Manager> manager_;
};

} // namespace gungnir::storage
