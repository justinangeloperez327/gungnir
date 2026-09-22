#include <gungnir/core/container.hpp>

#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>
#include <algorithm>

namespace gungnir {

class Container::Impl {
public:
    struct Binding {
        ErasedFactory factory;
        Lifetime lifetime{Lifetime::transient};
        std::shared_ptr<void> instance;
    };

    mutable std::mutex mutex;
    std::unordered_map<std::type_index, Binding> bindings;
    std::unordered_map<std::type_index, std::shared_ptr<void>> scoped_instances;
    bool scope_active{false};
    std::vector<std::type_index> resolving;
};

Container::Container() : impl_(std::make_unique<Impl>()) {}
Container::~Container() = default;
Container::Container(Container&&) noexcept = default;
Container& Container::operator=(Container&&) noexcept = default;

void Container::register_factory(
    std::type_index type,
    ErasedFactory factory,
    Lifetime lifetime
) {
    if (!factory) {
        throw std::invalid_argument("Gungnir container factory cannot be empty");
    }

    std::lock_guard lock{impl_->mutex};
    impl_->bindings.insert_or_assign(
        type,
        Impl::Binding{
            .factory = std::move(factory),
            .lifetime = lifetime,
            .instance = {}
        }
    );
}

void Container::register_instance(
    std::type_index type,
    std::shared_ptr<void> instance
) {
    std::lock_guard lock{impl_->mutex};
    impl_->bindings.insert_or_assign(
        type,
        Impl::Binding{
            .factory = {},
            .lifetime = Lifetime::singleton,
            .instance = std::move(instance)
        }
    );
}

std::shared_ptr<void> Container::resolve_erased(std::type_index type) {
    ErasedFactory factory;
    Lifetime lifetime = Lifetime::transient;

    {
        std::lock_guard lock{impl_->mutex};

        const auto found = impl_->bindings.find(type);
        if (found == impl_->bindings.end()) {
            throw std::logic_error("Gungnir container service is not registered");
        }

        if (found->second.instance) {
            return found->second.instance;
        }

        factory = found->second.factory;
        lifetime = found->second.lifetime;
        if (lifetime == Lifetime::scoped) {
            const auto scoped = impl_->scoped_instances.find(type);
            if (scoped != impl_->scoped_instances.end()) return scoped->second;
            if (!impl_->scope_active) throw std::logic_error("Scoped service resolved outside an active scope");
        }
    }

    auto created = factory(*this);
    if (!created) {
        throw std::logic_error("Gungnir container factory returned null");
    }

    if (lifetime == Lifetime::transient) return created;

    std::lock_guard lock{impl_->mutex};
    if (lifetime == Lifetime::scoped) {
        auto [it, inserted] = impl_->scoped_instances.emplace(type, created);
        return it->second;
    }
    auto& binding = impl_->bindings.at(type);
    if (!binding.instance) binding.instance = created;
    return binding.instance;
}

void Container::begin_scope() {
    std::lock_guard lock{impl_->mutex};
    impl_->scoped_instances.clear();
    impl_->scope_active = true;
}

void Container::end_scope() noexcept {
    std::lock_guard lock{impl_->mutex};
    impl_->scoped_instances.clear();
    impl_->scope_active = false;
}

bool Container::in_scope() const noexcept {
    std::lock_guard lock{impl_->mutex};
    return impl_->scope_active;
}

void Container::erase(std::type_index type) {
    std::lock_guard lock{impl_->mutex};
    impl_->bindings.erase(type);
    impl_->scoped_instances.erase(type);
}

void Container::enter_resolution(std::type_index type) {
    std::lock_guard lock{impl_->mutex};
    if (std::find(impl_->resolving.begin(), impl_->resolving.end(), type) != impl_->resolving.end()) {
        throw std::logic_error("Circular dependency detected while resolving Gungnir service");
    }
    impl_->resolving.push_back(type);
}

void Container::leave_resolution(std::type_index type) noexcept {
    std::lock_guard lock{impl_->mutex};
    const auto found = std::find(impl_->resolving.rbegin(), impl_->resolving.rend(), type);
    if (found != impl_->resolving.rend()) impl_->resolving.erase(std::next(found).base());
}

bool Container::contains(std::type_index type) const noexcept {
    std::lock_guard lock{impl_->mutex};
    return impl_->bindings.contains(type);
}

} // namespace gungnir
