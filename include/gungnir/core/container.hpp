#pragma once

#include <concepts>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeindex>
#include <type_traits>
#include <utility>

namespace gungnir {

class Container;

class ServiceScope {
public:
    ServiceScope() noexcept = default;

    template <typename Service>
    [[nodiscard]]
    std::shared_ptr<Service> resolve();

    [[nodiscard]]
    bool valid() const noexcept {
        return
            container_ != nullptr &&
            static_cast<bool>(state_);
    }

private:
    friend class Container;

    struct State;

    ServiceScope(
        Container& container,
        std::shared_ptr<State> state
    ) noexcept
        : container_(&container),
          state_(std::move(state)) {}

    Container* container_{nullptr};
    std::shared_ptr<State> state_;
};

class Container {
public:
    enum class Lifetime {
        transient,
        singleton,
        scoped
    };

    Container();
    ~Container();

    Container(Container&&) noexcept;
    Container& operator=(Container&&) noexcept;

    Container(const Container&) = delete;
    Container& operator=(const Container&) = delete;

    template <
        typename Service,
        typename Implementation = Service
    >
    void bind() {
        static_assert(
            std::same_as<
                Service,
                Implementation
            > ||
            std::derived_from<
                Implementation,
                Service
            >,
            "Implementation must be Service or derive from Service"
        );

        register_factory(
            std::type_index{
                typeid(Service)
            },
            [](
                Container&,
                ServiceScope*
            ) -> std::shared_ptr<void> {
                auto implementation =
                    std::make_shared<
                        Implementation
                    >();

                std::shared_ptr<Service>
                    service =
                        implementation;

                return
                    std::static_pointer_cast<
                        void
                    >(
                        std::move(service)
                    );
            },
            Lifetime::transient
        );
    }

    template <
        typename Service,
        typename Factory
    >
    void bind(
        Factory&& factory
    ) {
        register_factory(
            std::type_index{
                typeid(Service)
            },
            make_factory<Service>(
                std::forward<Factory>(
                    factory
                )
            ),
            Lifetime::transient
        );
    }

    template <
        typename Service,
        typename Implementation = Service
    >
    void singleton() {
        static_assert(
            std::same_as<
                Service,
                Implementation
            > ||
            std::derived_from<
                Implementation,
                Service
            >,
            "Implementation must be Service or derive from Service"
        );

        register_factory(
            std::type_index{
                typeid(Service)
            },
            [](
                Container&,
                ServiceScope*
            ) -> std::shared_ptr<void> {
                auto implementation =
                    std::make_shared<
                        Implementation
                    >();

                std::shared_ptr<Service>
                    service =
                        implementation;

                return
                    std::static_pointer_cast<
                        void
                    >(
                        std::move(service)
                    );
            },
            Lifetime::singleton
        );
    }

    template <
        typename Service,
        typename Factory
    >
    void singleton(
        Factory&& factory
    ) {
        register_factory(
            std::type_index{
                typeid(Service)
            },
            make_factory<Service>(
                std::forward<Factory>(
                    factory
                )
            ),
            Lifetime::singleton
        );
    }

    template <
        typename Service,
        typename Implementation = Service
    >
    void scoped() {
        static_assert(
            std::same_as<
                Service,
                Implementation
            > ||
            std::derived_from<
                Implementation,
                Service
            >,
            "Implementation must be Service or derive from Service"
        );

        register_factory(
            std::type_index{
                typeid(Service)
            },
            [](
                Container&,
                ServiceScope*
            ) -> std::shared_ptr<void> {
                auto implementation =
                    std::make_shared<
                        Implementation
                    >();

                std::shared_ptr<Service>
                    service =
                        implementation;

                return
                    std::static_pointer_cast<
                        void
                    >(
                        std::move(service)
                    );
            },
            Lifetime::scoped
        );
    }

    template <
        typename Service,
        typename Factory
    >
    void scoped(
        Factory&& factory
    ) {
        register_factory(
            std::type_index{
                typeid(Service)
            },
            make_factory<Service>(
                std::forward<Factory>(
                    factory
                )
            ),
            Lifetime::scoped
        );
    }

    template <typename Service>
    void instance(
        std::shared_ptr<Service> value
    ) {
        if (!value) {
            throw std::invalid_argument(
                "Gungnir container instance cannot be null"
            );
        }

        register_instance(
            std::type_index{
                typeid(Service)
            },
            std::static_pointer_cast<
                void
            >(
                std::move(value)
            )
        );
    }

    [[nodiscard]]
    ServiceScope scope();

    template <typename Service>
    [[nodiscard]]
    std::shared_ptr<Service> resolve() {
        return resolve_service<Service>(
            nullptr
        );
    }

    template <typename Service>
    [[nodiscard]]
    std::shared_ptr<Service> resolve(
        ServiceScope& scope
    ) {
        validate_scope(scope);

        return resolve_service<Service>(
            &scope
        );
    }

    template <
        typename Service,
        typename Target
    >
    void alias() {
        bind<Service>(
            [](
                Container& container,
                ServiceScope* scope
            ) {
                if (scope != nullptr) {
                    return
                        scope->template resolve<
                            Target
                        >();
                }

                return
                    container.template resolve<
                        Target
                    >();
            }
        );
    }

    template <typename Service>
    void forget() {
        erase(
            std::type_index{
                typeid(Service)
            }
        );
    }

    template <typename Service>
    [[nodiscard]]
    bool has() const noexcept {
        return contains(
            std::type_index{
                typeid(Service)
            }
        );
    }

private:
    using ErasedFactory =
        std::function<
            std::shared_ptr<void>(
                Container&,
                ServiceScope*
            )
        >;

    class Impl;
    std::unique_ptr<Impl> impl_;

    class ResolutionGuard {
    public:
        ResolutionGuard(
            Container& container,
            ServiceScope* scope,
            std::type_index type
        )
            : container_(container),
              scope_(scope),
              type_(type) {
            container_.enter_resolution(
                type_,
                scope_
            );
        }

        ~ResolutionGuard() {
            container_.leave_resolution(
                type_,
                scope_
            );
        }

        ResolutionGuard(
            const ResolutionGuard&
        ) = delete;

        ResolutionGuard& operator=(
            const ResolutionGuard&
        ) = delete;

    private:
        Container& container_;
        ServiceScope* scope_;
        std::type_index type_;
    };

    template <typename Service>
    [[nodiscard]]
    std::shared_ptr<Service>
    resolve_service(
        ServiceScope* scope
    ) {
        const auto key =
            std::type_index{
                typeid(Service)
            };

        ResolutionGuard guard{
            *this,
            scope,
            key
        };

        if (!contains(key)) {
            if constexpr (
                std::constructible_from<
                    Service,
                    ServiceScope&
                >
            ) {
                if (scope != nullptr) {
                    return
                        std::make_shared<
                            Service
                        >(*scope);
                }
            }

            if constexpr (
                std::constructible_from<
                    Service,
                    Container&
                >
            ) {
                return
                    std::make_shared<
                        Service
                    >(*this);
            }

            if constexpr (
                std::default_initializable<
                    Service
                >
            ) {
                return
                    std::make_shared<
                        Service
                    >();
            }

            throw std::logic_error(
                "Service is not registered and cannot be constructed automatically"
            );
        }

        return
            std::static_pointer_cast<
                Service
            >(
                resolve_erased(
                    key,
                    scope
                )
            );
    }

    void register_factory(
        std::type_index type,
        ErasedFactory factory,
        Lifetime lifetime
    );

    void register_instance(
        std::type_index type,
        std::shared_ptr<void> instance
    );

    [[nodiscard]]
    std::shared_ptr<void>
    resolve_erased(
        std::type_index type,
        ServiceScope* scope
    );

    [[nodiscard]]
    bool contains(
        std::type_index type
    ) const noexcept;

    void erase(
        std::type_index type
    );

    void validate_scope(
        const ServiceScope& scope
    ) const;

    void enter_resolution(
        std::type_index type,
        ServiceScope* scope
    );

    void leave_resolution(
        std::type_index type,
        ServiceScope* scope
    ) noexcept;

    template <typename>
    static constexpr bool
        always_false = false;

    template <
        typename Service,
        typename Result
    >
    [[nodiscard]]
    static std::shared_ptr<void>
    erase_result(
        Result&& result
    ) {
        using ResultType =
            std::remove_cvref_t<
                Result
            >;

        if constexpr (
            std::convertible_to<
                ResultType,
                std::shared_ptr<
                    Service
                >
            >
        ) {
            std::shared_ptr<Service>
                service =
                    std::forward<
                        Result
                    >(result);

            return
                std::static_pointer_cast<
                    void
                >(
                    std::move(service)
                );
        } else if constexpr (
            std::same_as<
                ResultType,
                Service
            > ||
            std::derived_from<
                ResultType,
                Service
            >
        ) {
            auto implementation =
                std::make_shared<
                    ResultType
                >(
                    std::forward<
                        Result
                    >(result)
                );

            std::shared_ptr<Service>
                service =
                    implementation;

            return
                std::static_pointer_cast<
                    void
                >(
                    std::move(service)
                );
        } else {
            static_assert(
                always_false<
                    ResultType
                >,
                "Container factory must return Service, a derived type, or shared_ptr<Service>"
            );
        }
    }

    template <
        typename Service,
        typename Factory
    >
    [[nodiscard]]
    static ErasedFactory
    make_factory(
        Factory&& factory
    ) {
        using StoredFactory =
            std::decay_t<Factory>;

        return [
            factory =
                StoredFactory(
                    std::forward<
                        Factory
                    >(factory)
                )
        ](
            Container& container,
            ServiceScope* scope
        ) mutable -> std::shared_ptr<void> {
            if constexpr (
                std::invocable<
                    StoredFactory&,
                    Container&,
                    ServiceScope*
                >
            ) {
                return erase_result<
                    Service
                >(
                    std::invoke(
                        factory,
                        container,
                        scope
                    )
                );
            }

            if constexpr (
                std::invocable<
                    StoredFactory&,
                    ServiceScope&
                >
            ) {
                if (scope != nullptr) {
                    return
                        erase_result<
                            Service
                        >(
                            std::invoke(
                                factory,
                                *scope
                            )
                        );
                }
            }

            if constexpr (
                std::invocable<
                    StoredFactory&,
                    Container&
                >
            ) {
                return
                    erase_result<
                        Service
                    >(
                        std::invoke(
                            factory,
                            container
                        )
                    );
            }

            if constexpr (
                std::invocable<
                    StoredFactory&
                >
            ) {
                return
                    erase_result<
                        Service
                    >(
                        std::invoke(
                            factory
                        )
                    );
            }

            if constexpr (
                std::invocable<
                    StoredFactory&,
                    ServiceScope&
                >
            ) {
                throw std::logic_error(
                    "Container factory requires a request service scope"
                );
            } else {
                static_assert(
                    always_false<
                        StoredFactory
                    >,
                    "Container factory must be callable with ServiceScope&, Container&, Container& plus ServiceScope*, or no arguments"
                );
            }
        };
    }
};

template <typename Service>
std::shared_ptr<Service>
ServiceScope::resolve() {
    if (!valid()) {
        throw std::logic_error(
            "Gungnir service scope is not attached to a container"
        );
    }

    return
        container_->template resolve<
            Service
        >(*this);
}

} // namespace gungnir
