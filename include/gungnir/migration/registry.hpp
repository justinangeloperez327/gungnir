#pragma once

#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <vector>

#include <gungnir/migration/error.hpp>
#include <gungnir/migration/runner.hpp>

namespace gungnir::migration {

class Registry {
public:
    Registry& add(String name, Migration& migration) {
        if (name.empty()) {
            throw RegistrationError{"Migration name cannot be empty"};
        }
        if (!names_.insert(name).second) {
            throw RegistrationError{"Duplicate migration '" + name + "'"};
        }
        migrations_.push_back(Named{std::move(name), &migration});
        return *this;
    }

    [[nodiscard]] const std::vector<Named>& all() const noexcept {
        return migrations_;
    }

    [[nodiscard]] bool empty() const noexcept {
        return migrations_.empty();
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return migrations_.size();
    }

private:
    std::unordered_set<String> names_;
    std::vector<Named> migrations_;
};

} // namespace gungnir::migration
