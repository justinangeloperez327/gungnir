#include <gungnir/core/application.hpp>

#include <stdexcept>
#include <utility>

#include <gungnir/database/runtime.hpp>
#include <gungnir/routing/route.hpp>

namespace gungnir {

class Application::Impl {
public:
    Container container;
    routing::Router router;
    database::Manager database;
    bool booted{false};
};

Application::Application()
    : impl_(std::make_unique<Impl>()) {
    routing::detail::bind_route_runtime(
        impl_->router,
        impl_->container
    );
}

Application::~Application() {
    if (impl_) {
        routing::detail::unbind_route_runtime(
            impl_->router,
            impl_->container
        );
    }

    shutdown();
}

Application::Application(Application&& other) noexcept
    : impl_(std::move(other.impl_)) {
    if (impl_) {
        routing::detail::bind_route_runtime(
            impl_->router,
            impl_->container
        );
    }

    if (impl_ && impl_->booted) {
        database::runtime::use(impl_->database);
    }
}

Application& Application::operator=(Application&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    if (impl_) {
        routing::detail::unbind_route_runtime(
            impl_->router,
            impl_->container
        );
    }

    shutdown();
    impl_ = std::move(other.impl_);

    if (impl_) {
        routing::detail::bind_route_runtime(
            impl_->router,
            impl_->container
        );
    }

    if (impl_ && impl_->booted) {
        database::runtime::use(impl_->database);
    }

    return *this;
}

Container& Application::container() noexcept {
    return impl_->container;
}

const Container& Application::container() const noexcept {
    return impl_->container;
}

routing::Router& Application::router() noexcept {
    return impl_->router;
}

const routing::Router& Application::router() const noexcept {
    return impl_->router;
}

database::Manager& Application::database() noexcept {
    return impl_->database;
}

const database::Manager& Application::database() const noexcept {
    return impl_->database;
}

Application& Application::database(
    String name,
    database::Backend backend,
    database::DriverFactory factory,
    std::size_t pool_size
) {
    impl_->database.add(
        std::move(name),
        backend,
        std::move(factory),
        pool_size
    );

    return *this;
}

void Application::boot() {
    if (impl_->booted) {
        throw std::logic_error(
            "Gungnir application is already booted"
        );
    }

    database::runtime::use(impl_->database);
    impl_->booted = true;
}

void Application::shutdown() noexcept {
    if (!impl_ || !impl_->booted) {
        return;
    }

    if (database::runtime::using_manager(impl_->database)) {
        database::runtime::clear();
    }

    impl_->booted = false;
}

bool Application::is_booted() const noexcept {
    return impl_ && impl_->booted;
}

} // namespace gungnir
