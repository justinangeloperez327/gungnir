#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <gungnir/storage/disk.hpp>

namespace gungnir::storage {

class Manager {
public:
    Manager& add(std::string name, std::shared_ptr<Disk> disk) {
        disks_.insert_or_assign(std::move(name), std::move(disk));
        return *this;
    }

    Manager& default_disk(std::string name) {
        default_ = std::move(name);
        return *this;
    }

    [[nodiscard]] Disk& disk(std::string_view name = {}) const {
        const auto selected = name.empty() ? default_ : std::string{name};
        const auto found = disks_.find(selected);
        if (found == disks_.end() || !found->second) {
            throw std::logic_error("Gungnir storage disk is not configured: " + selected);
        }
        return *found->second;
    }

private:
    std::string default_{"local"};
    std::unordered_map<std::string, std::shared_ptr<Disk>> disks_;
};

} // namespace gungnir::storage
