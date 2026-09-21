#pragma once

#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include <gungnir/core/types.hpp>

namespace gungnir {

class RelationNotLoaded : public std::logic_error {
public:
    RelationNotLoaded()
        : std::logic_error(
            "Gungnir relation is not loaded; eager-load it before access"
        ) {}
};

template <typename Related>
class SingleRelationState {
public:
    using related_type = Related;

    [[nodiscard]] bool loaded() const noexcept {
        return loaded_;
    }

    [[nodiscard]] bool empty() const noexcept {
        return loaded_ && !value_;
    }

    void set(Related value) {
        value_ = std::make_shared<Related>(std::move(value));
        loaded_ = true;
    }

    void set(std::shared_ptr<Related> value) noexcept {
        value_ = std::move(value);
        loaded_ = true;
    }

    void clear() noexcept {
        value_.reset();
        loaded_ = true;
    }

    void unload() noexcept {
        value_.reset();
        loaded_ = false;
    }

    [[nodiscard]] Related& get() {
        ensure_loaded();
        if (!value_) {
            throw std::logic_error("Gungnir relation is loaded but empty");
        }
        return *value_;
    }

    [[nodiscard]] const Related& get() const {
        ensure_loaded();
        if (!value_) {
            throw std::logic_error("Gungnir relation is loaded but empty");
        }
        return *value_;
    }

    [[nodiscard]] std::shared_ptr<Related> value() const {
        ensure_loaded();
        return value_;
    }

protected:
    void ensure_loaded() const {
        if (!loaded_) {
            throw RelationNotLoaded{};
        }
    }

private:
    bool loaded_{false};
    std::shared_ptr<Related> value_;
};

template <typename Related>
class ManyRelationState {
public:
    using related_type = Related;
    using collection_type = std::vector<Related>;

    [[nodiscard]] bool loaded() const noexcept {
        return loaded_;
    }

    [[nodiscard]] bool empty() const {
        ensure_loaded();
        return !values_ || values_->empty();
    }

    [[nodiscard]] std::size_t size() const {
        ensure_loaded();
        return values_ ? values_->size() : 0;
    }

    void set(collection_type values) {
        values_ = std::make_shared<collection_type>(std::move(values));
        loaded_ = true;
    }

    void clear() {
        values_ = std::make_shared<collection_type>();
        loaded_ = true;
    }

    void unload() noexcept {
        values_.reset();
        loaded_ = false;
    }

    [[nodiscard]] collection_type& get() {
        ensure_loaded();
        return *values_;
    }

    [[nodiscard]] const collection_type& get() const {
        ensure_loaded();
        return *values_;
    }

protected:
    void ensure_loaded() const {
        if (!loaded_) {
            throw RelationNotLoaded{};
        }
    }

private:
    bool loaded_{false};
    std::shared_ptr<collection_type> values_;
};

template <typename Related>
class HasOne : public SingleRelationState<Related> {
public:
    HasOne() = default;

    explicit HasOne(
        String foreign_key,
        String local_key = "id"
    )
        : foreign_key_(std::move(foreign_key)),
          local_key_(std::move(local_key)) {}

    [[nodiscard]] const String& foreign_key() const noexcept {
        return foreign_key_;
    }

    [[nodiscard]] const String& local_key() const noexcept {
        return local_key_;
    }

private:
    String foreign_key_;
    String local_key_{"id"};
};

template <typename Related>
class HasMany : public ManyRelationState<Related> {
public:
    HasMany() = default;

    explicit HasMany(
        String foreign_key,
        String local_key = "id"
    )
        : foreign_key_(std::move(foreign_key)),
          local_key_(std::move(local_key)) {}

    [[nodiscard]] const String& foreign_key() const noexcept {
        return foreign_key_;
    }

    [[nodiscard]] const String& local_key() const noexcept {
        return local_key_;
    }

private:
    String foreign_key_;
    String local_key_{"id"};
};

template <typename Related>
class BelongsTo : public SingleRelationState<Related> {
public:
    BelongsTo() = default;

    explicit BelongsTo(
        String foreign_key,
        String owner_key = "id"
    )
        : foreign_key_(std::move(foreign_key)),
          owner_key_(std::move(owner_key)) {}

    [[nodiscard]] const String& foreign_key() const noexcept {
        return foreign_key_;
    }

    [[nodiscard]] const String& owner_key() const noexcept {
        return owner_key_;
    }

private:
    String foreign_key_;
    String owner_key_{"id"};
};

template <typename Related>
class BelongsToMany : public ManyRelationState<Related> {
public:
    BelongsToMany() = default;

    BelongsToMany(
        String pivot_table,
        String foreign_pivot_key,
        String related_pivot_key,
        String parent_key = "id",
        String related_key = "id"
    )
        : pivot_table_(std::move(pivot_table)),
          foreign_pivot_key_(std::move(foreign_pivot_key)),
          related_pivot_key_(std::move(related_pivot_key)),
          parent_key_(std::move(parent_key)),
          related_key_(std::move(related_key)) {}

    [[nodiscard]] const String& pivot_table() const noexcept {
        return pivot_table_;
    }

    [[nodiscard]] const String& foreign_pivot_key() const noexcept {
        return foreign_pivot_key_;
    }

    [[nodiscard]] const String& related_pivot_key() const noexcept {
        return related_pivot_key_;
    }

    [[nodiscard]] const String& parent_key() const noexcept {
        return parent_key_;
    }

    [[nodiscard]] const String& related_key() const noexcept {
        return related_key_;
    }

private:
    String pivot_table_;
    String foreign_pivot_key_;
    String related_pivot_key_;
    String parent_key_{"id"};
    String related_key_{"id"};
};

template <typename Related, typename Through>
class HasOneThrough : public SingleRelationState<Related> {
public:
    using through_type = Through;

    HasOneThrough() = default;

    HasOneThrough(
        String first_key,
        String second_key,
        String local_key = "id",
        String second_local_key = "id"
    )
        : first_key_(std::move(first_key)),
          second_key_(std::move(second_key)),
          local_key_(std::move(local_key)),
          second_local_key_(std::move(second_local_key)) {}

    [[nodiscard]] const String& first_key() const noexcept {
        return first_key_;
    }

    [[nodiscard]] const String& second_key() const noexcept {
        return second_key_;
    }

    [[nodiscard]] const String& local_key() const noexcept {
        return local_key_;
    }

    [[nodiscard]] const String& second_local_key() const noexcept {
        return second_local_key_;
    }

private:
    String first_key_;
    String second_key_;
    String local_key_{"id"};
    String second_local_key_{"id"};
};

template <typename Related, typename Through>
class HasManyThrough : public ManyRelationState<Related> {
public:
    using through_type = Through;

    HasManyThrough() = default;

    HasManyThrough(
        String first_key,
        String second_key,
        String local_key = "id",
        String second_local_key = "id"
    )
        : first_key_(std::move(first_key)),
          second_key_(std::move(second_key)),
          local_key_(std::move(local_key)),
          second_local_key_(std::move(second_local_key)) {}

    [[nodiscard]] const String& first_key() const noexcept {
        return first_key_;
    }

    [[nodiscard]] const String& second_key() const noexcept {
        return second_key_;
    }

    [[nodiscard]] const String& local_key() const noexcept {
        return local_key_;
    }

    [[nodiscard]] const String& second_local_key() const noexcept {
        return second_local_key_;
    }

private:
    String first_key_;
    String second_key_;
    String local_key_{"id"};
    String second_local_key_{"id"};
};

} // namespace gungnir
