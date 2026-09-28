#include <gungnir/core/container.hpp>

#include <algorithm>
#include <iterator>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gungnir {

struct ServiceScope::State {
    mutable std::mutex mutex;

    std::unordered_map<
        std::type_index,
        std::shared_ptr<void>
    > instances;

    std::unordered_map<
        std::thread::id,
        std::vector<
            std::type_index
        >
    > resolving;
};

class Container::Impl {
public:
    struct Binding {
        ErasedFactory factory;
        Lifetime lifetime{
            Lifetime::transient
        };
        std::shared_ptr<void> instance;
    };

    mutable std::mutex mutex;

    std::unordered_map<
        std::type_index,
        Binding
    > bindings;

    std::unordered_map<
        std::thread::id,
        std::vector<
            std::type_index
        >
    > resolving;
};

Container::Container()
    : impl_(
        std::make_unique<Impl>()
      ) {}

Container::~Container() = default;

Container::Container(
    Container&&
) noexcept = default;

Container& Container::operator=(
    Container&&
) noexcept = default;

ServiceScope Container::scope() {
    return ServiceScope{
        *this,
        std::make_shared<
            ServiceScope::State
        >()
    };
}

void Container::register_factory(
    std::type_index type,
    ErasedFactory factory,
    Lifetime lifetime
) {
    if (!factory) {
        throw std::invalid_argument(
            "Gungnir container factory cannot be empty"
        );
    }

    std::lock_guard lock{
        impl_->mutex
    };

    impl_->bindings.insert_or_assign(
        type,
        Impl::Binding{
            .factory =
                std::move(factory),
            .lifetime =
                lifetime,
            .instance = {}
        }
    );
}

void Container::register_instance(
    std::type_index type,
    std::shared_ptr<void> instance
) {
    std::lock_guard lock{
        impl_->mutex
    };

    impl_->bindings.insert_or_assign(
        type,
        Impl::Binding{
            .factory = {},
            .lifetime =
                Lifetime::singleton,
            .instance =
                std::move(instance)
        }
    );
}

std::shared_ptr<void>
Container::resolve_erased(
    std::type_index type,
    ServiceScope* scope
) {
    ErasedFactory factory;
    Lifetime lifetime =
        Lifetime::transient;

    {
        std::lock_guard lock{
            impl_->mutex
        };

        const auto found =
            impl_->bindings.find(
                type
            );

        if (
            found ==
            impl_->bindings.end()
        ) {
            throw std::logic_error(
                "Gungnir container service is not registered"
            );
        }

        if (
            found->second.instance
        ) {
            return
                found->second.instance;
        }

        factory =
            found->second.factory;

        lifetime =
            found->second.lifetime;
    }

    if (
        lifetime ==
        Lifetime::scoped
    ) {
        if (scope == nullptr) {
            throw std::logic_error(
                "Scoped service resolved outside a ServiceScope"
            );
        }

        validate_scope(
            *scope
        );

        {
            std::lock_guard lock{
                scope->state_->mutex
            };

            const auto found =
                scope->state_->
                    instances.find(
                        type
                    );

            if (
                found !=
                scope->state_->
                    instances.end()
            ) {
                return found->second;
            }
        }
    }

    auto created =
        factory(
            *this,
            scope
        );

    if (!created) {
        throw std::logic_error(
            "Gungnir container factory returned null"
        );
    }

    if (
        lifetime ==
        Lifetime::transient
    ) {
        return created;
    }

    if (
        lifetime ==
        Lifetime::scoped
    ) {
        std::lock_guard lock{
            scope->state_->mutex
        };

        auto [
            found,
            inserted
        ] =
            scope->state_->
                instances.emplace(
                    type,
                    created
                );

        static_cast<void>(
            inserted
        );

        return found->second;
    }

    std::lock_guard lock{
        impl_->mutex
    };

    auto& binding =
        impl_->bindings.at(
            type
        );

    if (!binding.instance) {
        binding.instance =
            std::move(created);
    }

    return binding.instance;
}

void Container::validate_scope(
    const ServiceScope& scope
) const {
    if (
        scope.container_ != this ||
        !scope.state_
    ) {
        throw std::invalid_argument(
            "ServiceScope belongs to a different container"
        );
    }
}

void Container::erase(
    std::type_index type
) {
    std::lock_guard lock{
        impl_->mutex
    };

    impl_->bindings.erase(
        type
    );
}

bool Container::contains(
    std::type_index type
) const noexcept {
    std::lock_guard lock{
        impl_->mutex
    };

    return
        impl_->bindings.contains(
            type
        );
}

void Container::enter_resolution(
    std::type_index type,
    ServiceScope* scope
) {
    const auto thread =
        std::this_thread::get_id();

    if (scope != nullptr) {
        validate_scope(
            *scope
        );

        std::lock_guard lock{
            scope->state_->mutex
        };

        auto& resolving =
            scope->state_->
                resolving[thread];

        if (
            std::find(
                resolving.begin(),
                resolving.end(),
                type
            ) !=
            resolving.end()
        ) {
            throw std::logic_error(
                "Circular dependency detected while resolving scoped service"
            );
        }

        resolving.push_back(
            type
        );

        return;
    }

    std::lock_guard lock{
        impl_->mutex
    };

    auto& resolving =
        impl_->resolving[thread];

    if (
        std::find(
            resolving.begin(),
            resolving.end(),
            type
        ) !=
        resolving.end()
    ) {
        throw std::logic_error(
            "Circular dependency detected while resolving Gungnir service"
        );
    }

    resolving.push_back(
        type
    );
}

void Container::leave_resolution(
    std::type_index type,
    ServiceScope* scope
) noexcept {
    const auto thread =
        std::this_thread::get_id();

    auto remove =
        [&](
            auto& resolving_map
        ) {
            const auto found =
                resolving_map.find(
                    thread
                );

            if (
                found ==
                resolving_map.end()
            ) {
                return;
            }

            auto& resolving =
                found->second;

            const auto entry =
                std::find(
                    resolving.rbegin(),
                    resolving.rend(),
                    type
                );

            if (
                entry !=
                resolving.rend()
            ) {
                resolving.erase(
                    std::next(entry)
                        .base()
                );
            }

            if (
                resolving.empty()
            ) {
                resolving_map.erase(
                    found
                );
            }
        };

    if (
        scope != nullptr &&
        scope->container_ == this &&
        scope->state_
    ) {
        std::lock_guard lock{
            scope->state_->mutex
        };

        remove(
            scope->state_->
                resolving
        );

        return;
    }

    std::lock_guard lock{
        impl_->mutex
    };

    remove(
        impl_->resolving
    );
}

} // namespace gungnir
