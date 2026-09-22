#pragma once

#include <concepts>
#include <functional>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <type_traits>
#include <utility>
#include <string>
#include <string_view>

namespace gungnir {

class Container {
public:
    enum class Lifetime { transient, singleton, scoped };
    Container();
    ~Container();

    Container(Container&&) noexcept;
    Container& operator=(Container&&) noexcept;

    Container(const Container&) = delete;
    Container& operator=(const Container&) = delete;

    template <typename Service, typename Implementation = Service>
    void bind() {
        static_assert(
            std::same_as<Service, Implementation> ||
            std::derived_from<Implementation, Service>,
            "Implementation must be Service or derive from Service"
        );

        register_factory(
            std::type_index{typeid(Service)},
            [](Container&) -> std::shared_ptr<void> {
                auto implementation = std::make_shared<Implementation>();
                std::shared_ptr<Service> service = implementation;
                return std::static_pointer_cast<void>(service);
            },
            false
        );
    }

    template <typename Service, typename Factory>
    void bind(Factory&& factory) {
        register_factory(
            std::type_index{typeid(Service)},
            make_factory<Service>(std::forward<Factory>(factory)),
            false
        );
    }

    template <typename Service, typename Implementation = Service>
    void singleton() {
        static_assert(
            std::same_as<Service, Implementation> ||
            std::derived_from<Implementation, Service>,
            "Implementation must be Service or derive from Service"
        );

        register_factory(
            std::type_index{typeid(Service)},
            [](Container&) -> std::shared_ptr<void> {
                auto implementation = std::make_shared<Implementation>();
                std::shared_ptr<Service> service = implementation;
                return std::static_pointer_cast<void>(service);
            },
            true
        );
    }

    template <typename Service, typename Factory>
    void singleton(Factory&& factory) {
        register_factory(
            std::type_index{typeid(Service)},
            make_factory<Service>(std::forward<Factory>(factory)),
            true
        );
    }

    template <typename Service>
    void instance(std::shared_ptr<Service> value) {
        if (!value) {
            throw std::invalid_argument("Gungnir container instance cannot be null");
        }

        register_instance(
            std::type_index{typeid(Service)},
            std::static_pointer_cast<void>(std::move(value))
        );
    }

    template <typename Service>
    [[nodiscard]] std::shared_ptr<Service> resolve() {
        const auto key = std::type_index{typeid(Service)};

        if (!contains(key)) {
            if constexpr (std::constructible_from<Service, Container&>) {
                return std::make_shared<Service>(*this);
            } else if constexpr (std::default_initializable<Service>) {
                return std::make_shared<Service>();
            }

            throw std::logic_error(
                "Service is not registered and cannot be constructed automatically"
            );
        }

        return std::static_pointer_cast<Service>(resolve_erased(key));
    }

    template <typename Service, typename Implementation = Service>
    void scoped() {
        static_assert(std::same_as<Service, Implementation> || std::derived_from<Implementation, Service>);
        register_factory(std::type_index{typeid(Service)}, [](Container&) -> std::shared_ptr<void> {
            auto implementation = std::make_shared<Implementation>();
            std::shared_ptr<Service> service = implementation;
            return std::static_pointer_cast<void>(service);
        }, Lifetime::scoped);
    }

    template <typename Service, typename Factory>
    void scoped(Factory&& factory) {
        register_factory(std::type_index{typeid(Service)}, make_factory<Service>(std::forward<Factory>(factory)), Lifetime::scoped);
    }

    template <typename Service, typename Target>
    void alias() {
        bind<Service>([](Container& container) { return container.template resolve<Target>(); });
    }

    template <typename Service>
    void forget() { erase(std::type_index{typeid(Service)}); }

    void begin_scope();
    void end_scope() noexcept;
    [[nodiscard]] bool in_scope() const noexcept;

    template <typename Service>
    [[nodiscard]] bool has() const noexcept {
        return contains(std::type_index{typeid(Service)});
    }

private:
    using ErasedFactory = std::function<std::shared_ptr<void>(Container&)>;

    class Impl;
    std::unique_ptr<Impl> impl_;

    void register_factory(std::type_index type, ErasedFactory factory, Lifetime lifetime);
    void register_factory(std::type_index type, ErasedFactory factory, bool singleton) {
        register_factory(type, std::move(factory), singleton ? Lifetime::singleton : Lifetime::transient);
    }
    void register_instance(std::type_index type, std::shared_ptr<void> instance);
    [[nodiscard]] std::shared_ptr<void> resolve_erased(std::type_index type);
    [[nodiscard]] bool contains(std::type_index type) const noexcept;
    void erase(std::type_index type);

    template <typename>
    static constexpr bool always_false = false;

    template <typename Service, typename Result>
    [[nodiscard]] static std::shared_ptr<void> erase(Result&& result) {
        using ResultType = std::remove_cvref_t<Result>;

        if constexpr (std::convertible_to<ResultType, std::shared_ptr<Service>>) {
            std::shared_ptr<Service> service = std::forward<Result>(result);
            return std::static_pointer_cast<void>(std::move(service));
        } else if constexpr (
            std::same_as<ResultType, Service> ||
            std::derived_from<ResultType, Service>
        ) {
            auto implementation =
                std::make_shared<ResultType>(std::forward<Result>(result));
            std::shared_ptr<Service> service = implementation;
            return std::static_pointer_cast<void>(std::move(service));
        } else {
            static_assert(
                always_false<ResultType>,
                "Container factory must return Service, a derived type, or shared_ptr<Service>"
            );
        }
    }

    template <typename Service, typename Factory>
    [[nodiscard]] static ErasedFactory make_factory(Factory&& factory) {
        using StoredFactory = std::decay_t<Factory>;

        return [factory = StoredFactory(std::forward<Factory>(factory))]
               (Container& container) mutable -> std::shared_ptr<void> {
            if constexpr (std::invocable<StoredFactory&, Container&>) {
                return erase<Service>(std::invoke(factory, container));
            } else if constexpr (std::invocable<StoredFactory&>) {
                return erase<Service>(std::invoke(factory));
            } else {
                static_assert(
                    always_false<StoredFactory>,
                    "Container factory must be callable with Container& or no arguments"
                );
            }
        };
    }
};

} // namespace gungnir
