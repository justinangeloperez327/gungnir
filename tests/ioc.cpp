#include <cassert>
#include <memory>
#include <utility>

#include <gungnir/gungnir.hpp>

class MessageBus {
public:
    virtual ~MessageBus() = default;
    [[nodiscard]] virtual gungnir::String name() const = 0;
};

class LocalMessageBus final : public MessageBus {
public:
    [[nodiscard]] gungnir::String name() const override {
        return "local";
    }
};

class Clock {
public:
    [[nodiscard]] gungnir::String name() const {
        return "clock";
    }
};

class Config {
public:
    explicit Config(gungnir::String environment)
        : environment(std::move(environment)) {}

    gungnir::String environment;
};

int main() {
    gungnir::Application app;

    app.bind<MessageBus, LocalMessageBus>();
    auto first_bus = app.resolve<MessageBus>();
    auto second_bus = app.resolve<MessageBus>();

    assert(first_bus->name() == "local");
    assert(first_bus != second_bus);

    app.singleton<Clock>();
    auto first_clock = app.resolve<Clock>();
    auto second_clock = app.resolve<Clock>();

    assert(first_clock == second_clock);
    assert(first_clock->name() == "clock");

    app.singleton<Config>([](gungnir::Container&) {
        return Config{"testing"};
    });

    auto config = app.resolve<Config>();
    assert(config->environment == "testing");

    auto supplied = std::make_shared<Config>("instance");
    app.instance<Config>(supplied);

    assert(app.resolve<Config>() == supplied);

    auto auto_created = app.resolve<LocalMessageBus>();
    assert(auto_created->name() == "local");

    app.router().get("/health", [] {
        return gungnir::Response::text("ok");
    });

    app.router().delete_("/users/1", [] {
        return gungnir::Response{204};
    });

    return 0;
}
