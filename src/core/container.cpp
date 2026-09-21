#include <gungnir/core/container.hpp>

#include <mutex>
#include <unordered_map>
#include <utility>

namespace gungnir {

class Container::Impl {
public:
    struct Binding {
        ErasedFactory factory;
        bool singleton{false};
        std::shared_ptr<void> instance;
    };

    mutable std::mutex mutex;
    std::unordered_map<std::type_index, Binding> bindings;
};

Container::Container() : impl_(std::make_unique<Impl>()) {}
Container::~Container() = default;
Container::Container(Container&&) noexcept = default;
Container& Container::operator=(Container&&) noexcept = default;

void Container::register_factory(
    std::type_index type,
    ErasedFactory factory,
    bool singleton
) {
    if (!factory) {
        throw std::invalid_argument("Gungnir container factory cannot be empty");
    }

    std::lock_guard lock{impl_->mutex};
    impl_->bindings.insert_or_assign(
        type,
        Impl::Binding{
            .factory = std::move(factory),
            .singleton = singleton,
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
            .singleton = true,
            .instance = std::move(instance)
        }
    );
}

std::shared_ptr<void> Container::resolve_erased(std::type_index type) {
    ErasedFactory factory;
    bool singleton = false;

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
        singleton = found->second.singleton;
    }

    auto created = factory(*this);
    if (!created) {
        throw std::logic_error("Gungnir container factory returned null");
    }

    if (!singleton) {
        return created;
    }

    std::lock_guard lock{impl_->mutex};
    auto& binding = impl_->bindings.at(type);

    if (!binding.instance) {
        binding.instance = created;
    }

    return binding.instance;
}

bool Container::contains(std::type_index type) const noexcept {
    std::lock_guard lock{impl_->mutex};
    return impl_->bindings.contains(type);
}

} // namespace gungnir
