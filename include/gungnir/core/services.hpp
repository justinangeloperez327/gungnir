#pragma once
#include <gungnir/core/application.hpp>
#include <gungnir/cache/repository.hpp>
#include <gungnir/cache/values.hpp>
#include <gungnir/queue/dispatcher.hpp>
#include <gungnir/queue/worker.hpp>
#include <gungnir/scheduler/scheduler.hpp>
#include <gungnir/mail/transport.hpp>
#include <gungnir/mail/service.hpp>
#include <gungnir/events/service.hpp>
#include <gungnir/queue/service.hpp>
#include <gungnir/scheduler/service.hpp>
#include <gungnir/notifications/adapters.hpp>
#include <gungnir/notifications/service.hpp>
#include <gungnir/storage/manager.hpp>
#include <gungnir/storage/service.hpp>
#include <gungnir/auth/resource_authorization.hpp>
#include <gungnir/auth/login.hpp>
#include <gungnir/config/service.hpp>
#include <gungnir/logging/service.hpp>
#include <gungnir/observability/service.hpp>

namespace gungnir {
struct ServiceOptions {
    std::shared_ptr<cache::Store> cache;
    std::shared_ptr<queue::Driver> queue;
    std::shared_ptr<mail::Transport> mail;
    mail::Address sender;
    std::shared_ptr<storage::Manager> storage;
    bool database_notifications{false};
    std::shared_ptr<auth::SessionGuard> authentication;
    std::shared_ptr<scheduler::LockStore> scheduler_locks;
    queue::WorkerOptions worker_options;
    std::shared_ptr<logging::Logger> logger;
    std::shared_ptr<observability::Tracer> tracer;
    std::shared_ptr<observability::Meter> meter;
    std::shared_ptr<cache::LockStore> cache_locks;
};
// Explicit adapters are registered during application bootstrap. No network
// clients or worker threads are started merely by constructing an application.
class ServicesProvider final : public Provider {
public:
    explicit ServicesProvider(ServiceOptions options = {}) : options_(std::move(options)) {}
    void register_services(Application& app) override {
        auto& container = app.container();
        auto logger = options_.logger ? options_.logger : std::make_shared<logging::Logger>();
        auto tracer = options_.tracer ? options_.tracer : observability::global_tracer();
        auto meter = options_.meter ? options_.meter : observability::global_meter();
        container.instance<logging::Logger>(logger);
        container.instance<logging::Service>(std::make_shared<logging::Service>(std::move(logger)));
        container.instance<observability::Tracer>(tracer);
        container.instance<observability::Meter>(meter);
        container.instance<observability::Service>(std::make_shared<observability::Service>(tracer, meter));
        app.execution_context()->tracer = options_.tracer;
        app.execution_context()->meter = options_.meter;
        const auto cleanup = [](const auto& exporter) {
            std::exception_ptr failure;
            try { exporter->flush(); } catch (...) { failure = std::current_exception(); }
            try { exporter->shutdown(); } catch (...) { if (!failure) failure = std::current_exception(); }
            if (failure) std::rethrow_exception(failure);
        };
        if (options_.tracer) app.on_shutdown([tracer,cleanup](Application&) { cleanup(tracer); });
        if (options_.meter) app.on_shutdown([meter,cleanup](Application&) { cleanup(meter); });
        auto events = std::make_shared<events::Dispatcher>();
        container.instance<events::Dispatcher>(events);
        container.instance<events::Service>(std::make_shared<events::Service>(events));
        // Listeners may themselves inject Events. Release registrations during
        // shutdown to break the dispatcher -> listener -> dispatcher cycle.
        app.on_shutdown([owner = std::weak_ptr<events::Dispatcher>{events}](Application&) {
            if (auto dispatcher = owner.lock()) dispatcher->clear();
        });
        container.instance<auth::ResourceAuthorization>(std::make_shared<auth::ResourceAuthorization>());
        if (options_.authentication)
            container.instance<auth::SessionGuard>(options_.authentication);
        auto notifications = std::make_shared<notifications::Manager>();
        if (options_.cache) {
            container.instance<cache::Store>(options_.cache);
            // Aliasing owners preserve adapter lifetimes beyond the provider.
            struct RepositoryOwner { std::shared_ptr<cache::Store> store; cache::Repository repository; RepositoryOwner(std::shared_ptr<cache::Store> store, std::shared_ptr<cache::LockStore> locks) : store(std::move(store)), repository(*this->store, std::move(locks)) {} };
            auto owner = std::make_shared<RepositoryOwner>(options_.cache, options_.cache_locks);
            auto repository = std::shared_ptr<cache::Repository>{owner,&owner->repository};
            container.instance<cache::Repository>(repository);
            container.instance<cache::Values>(std::make_shared<cache::Values>(std::move(repository)));
        }
        if (options_.queue) {
            container.instance<queue::Driver>(options_.queue);
            auto dispatcher = std::make_shared<queue::Dispatcher>(options_.queue);
            container.instance<queue::Dispatcher>(dispatcher);
            container.instance<queue::Service>(std::make_shared<queue::Service>(dispatcher, options_.queue));
            struct WorkerOwner { std::shared_ptr<queue::Driver> driver; queue::Worker worker; WorkerOwner(std::shared_ptr<queue::Driver> driver, queue::WorkerOptions options) : driver(std::move(driver)), worker(*this->driver, std::move(options)) {} };
            auto owner = std::make_shared<WorkerOwner>(options_.queue, options_.worker_options);
            container.instance<queue::Worker>(std::shared_ptr<queue::Worker>{owner,&owner->worker});
        }
        struct SchedulerOwner { std::shared_ptr<scheduler::LockStore> locks; scheduler::SystemClock clock; scheduler::Scheduler scheduler{clock}; };
        auto schedule = std::make_shared<SchedulerOwner>();
        schedule->locks = options_.scheduler_locks;
        auto native_schedule = std::shared_ptr<scheduler::Scheduler>{schedule,&schedule->scheduler};
        container.instance<scheduler::Scheduler>(native_schedule);
        container.instance<scheduler::Service>(std::make_shared<scheduler::Service>(native_schedule,
            options_.queue ? container.resolve<queue::Dispatcher>() : nullptr, options_.scheduler_locks));
        if (options_.mail) {
            container.instance<mail::Transport>(options_.mail);
            struct MailerOwner { std::shared_ptr<mail::Transport> transport; mail::Mailer mailer; explicit MailerOwner(std::shared_ptr<mail::Transport> transport) : transport(std::move(transport)), mailer(*this->transport) {} };
            auto owner = std::make_shared<MailerOwner>(options_.mail);
            container.instance<mail::Mailer>(std::shared_ptr<mail::Mailer>{owner,&owner->mailer});
            container.instance<mail::Service>(std::make_shared<mail::Service>(options_.mail, options_.sender,
                options_.queue ? container.resolve<queue::Service>() : nullptr));
            notifications->channel("mail",std::make_shared<notifications::MailChannel>(options_.mail,options_.sender));
        }
        if (options_.database_notifications) notifications->channel("database",std::make_shared<notifications::DatabaseChannel>());
        container.instance<notifications::Manager>(notifications);
        container.instance<notifications::Service>(std::make_shared<notifications::Service>(std::move(notifications), options_.sender,
            options_.queue ? container.resolve<queue::Service>() : nullptr));
        if (options_.storage) {
            container.instance<storage::Manager>(options_.storage);
            container.instance<storage::Service>(std::make_shared<storage::Service>(options_.storage));
        }
        if (options_.queue) {
            auto worker = container.resolve<queue::Worker>();
            auto context = std::weak_ptr<detail::ExecutionContext>{app.execution_context()};
            // Weak application ownership avoids a worker/container cycle. A new
            // execution and dependency scope belongs to each delivery attempt.
            auto delivery_scope = [context] {
                auto owner = context.lock();
                if (!owner) throw std::logic_error("Delivery application is no longer available");
                auto current = std::make_shared<detail::ExecutionContext>(*owner);
                current->trace = observability::current_context();
                return current;
            };
            worker->handle(std::string{mail::delivery::Job::job_name}, [delivery_scope](std::string_view payload) {
                auto owner = delivery_scope(); detail::ExecutionScope active{owner};
                auto scope = owner->container->scope();
                scope.resolve<mail::Service>()->deliver(mail::delivery::Job::read(payload));
            });
            worker->handle(std::string{notifications::Delivery::job_name}, [delivery_scope](std::string_view payload) {
                auto owner = delivery_scope(); detail::ExecutionScope active{owner};
                auto scope = owner->container->scope();
                scope.resolve<notifications::Service>()->deliver(notifications::Snapshot::read(payload));
            });
        }
    }
private:
    ServiceOptions options_;
};
// Generated declaration adapters can be registered from on_boot/provider hooks.
template<class Job> void register_job(Application& app) {
    auto worker = app.container().resolve<queue::Worker>();
    auto owner = std::weak_ptr<Container>{app.execution_context()->container};
    if constexpr (requires { Job::register_job(*worker,owner); }) Job::register_job(*worker,owner);
    else if constexpr (requires { Job::register_job(*worker,app.container()); }) Job::register_job(*worker,app.container());
    else Job::register_job(*worker);
}
template<class Listener> void register_listener(Application& app,std::shared_ptr<Listener> listener) {
    Listener::register_listener(*app.container().resolve<events::Dispatcher>(),std::move(listener));
}
template<class Policy> void register_policy(Application& app,std::shared_ptr<Policy> policy) {
    Policy::register_policy(*app.container().resolve<auth::ResourceAuthorization>(),std::move(policy));
}
template<class Job> scheduler::Task& schedule_job(Application& app,std::string name,std::chrono::milliseconds interval,Job job) {
    auto dispatcher = app.container().resolve<queue::Dispatcher>();
    return app.container().resolve<scheduler::Scheduler>()->every(std::move(name),interval,[dispatcher,job=std::move(job)] { dispatcher->dispatch(job); });
}
}
