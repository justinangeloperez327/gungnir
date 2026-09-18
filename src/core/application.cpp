#include <gungnir/core/application.hpp>

#include <stdexcept>

namespace gungnir {

class Application::Impl {
public:
    routing::Router router;
    bool booted{false};
};

Application::Application() : impl_(std::make_unique<Impl>()) {}
Application::~Application() = default;
Application::Application(Application&&) noexcept = default;
Application& Application::operator=(Application&&) noexcept = default;

routing::Router& Application::router() noexcept { return impl_->router; }
const routing::Router& Application::router() const noexcept { return impl_->router; }

void Application::boot() {
    if (impl_->booted) {
        throw std::logic_error("Gungnir application is already booted");
    }
    impl_->booted = true;
}

void Application::shutdown() noexcept { impl_->booted = false; }

bool Application::is_booted() const noexcept { return impl_->booted; }

} // namespace gungnir
