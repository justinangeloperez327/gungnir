#pragma once
#include <gungnir/core/application.hpp>
#include <gungnir/cache/repository.hpp>
#include <gungnir/cache/values.hpp>
#include <gungnir/queue/dispatcher.hpp>
#include <gungnir/queue/worker.hpp>
#include <gungnir/scheduler/scheduler.hpp>
#include <gungnir/mail/transport.hpp>
#include <gungnir/events/dispatcher.hpp>
#include <gungnir/notifications/adapters.hpp>
#include <gungnir/storage/manager.hpp>
#include <gungnir/storage/service.hpp>
#include <gungnir/auth/resource_authorization.hpp>
#include <gungnir/auth/login.hpp>

namespace gungnir {
struct ServiceOptions {
    std::shared_ptr<cache::Store> cache;
    std::shared_ptr<queue::Driver> queue;
    std::shared_ptr<mail::Transport> mail;
    mail::Address sender;
    std::shared_ptr<storage::Manager> storage;
    bool database_notifications{false};
    std::shared_ptr<auth::SessionGuard> authentication;
};
// Explicit adapters are registered during application bootstrap. No network
// clients or worker threads are started merely by constructing an application.
class ServicesProvider final : public Provider {
public:
    explicit ServicesProvider(ServiceOptions options = {}) : options_(std::move(options)) {}
    void register_services(Application& app) override {
        auto& container = app.container();
        container.instance<events::Dispatcher>(std::make_shared<events::Dispatcher>());
        container.instance<auth::ResourceAuthorization>(std::make_shared<auth::ResourceAuthorization>());
        if (options_.authentication)
            container.instance<auth::SessionGuard>(options_.authentication);
        auto notifications = std::make_shared<notifications::Manager>();
        if (options_.cache) {
            container.instance<cache::Store>(options_.cache);
            // Aliasing owners preserve adapter lifetimes beyond the provider.
            struct RepositoryOwner { std::shared_ptr<cache::Store> store; cache::Repository repository; explicit RepositoryOwner(std::shared_ptr<cache::Store> store) : store(std::move(store)), repository(*this->store) {} };
            auto owner = std::make_shared<RepositoryOwner>(options_.cache);
            auto repository = std::shared_ptr<cache::Repository>{owner,&owner->repository};
            container.instance<cache::Repository>(repository);
            container.instance<cache::Values>(std::make_shared<cache::Values>(std::move(repository)));
        }
        if (options_.queue) {
            container.instance<queue::Driver>(options_.queue);
            container.instance<queue::Dispatcher>(std::make_shared<queue::Dispatcher>(options_.queue));
            struct WorkerOwner { std::shared_ptr<queue::Driver> driver; queue::Worker worker; explicit WorkerOwner(std::shared_ptr<queue::Driver> driver) : driver(std::move(driver)), worker(*this->driver) {} };
            auto owner = std::make_shared<WorkerOwner>(options_.queue);
            container.instance<queue::Worker>(std::shared_ptr<queue::Worker>{owner,&owner->worker});
        }
        struct SchedulerOwner { scheduler::SystemClock clock; scheduler::Scheduler scheduler{clock}; };
        auto schedule = std::make_shared<SchedulerOwner>();
        container.instance<scheduler::Scheduler>(std::shared_ptr<scheduler::Scheduler>{schedule,&schedule->scheduler});
        if (options_.mail) {
            container.instance<mail::Transport>(options_.mail);
            struct MailerOwner { std::shared_ptr<mail::Transport> transport; mail::Mailer mailer; explicit MailerOwner(std::shared_ptr<mail::Transport> transport) : transport(std::move(transport)), mailer(*this->transport) {} };
            auto owner = std::make_shared<MailerOwner>(options_.mail);
            container.instance<mail::Mailer>(std::shared_ptr<mail::Mailer>{owner,&owner->mailer});
            notifications->channel("mail",std::make_shared<notifications::MailChannel>(options_.mail,options_.sender));
        }
        if (options_.database_notifications) notifications->channel("database",std::make_shared<notifications::DatabaseChannel>());
        container.instance<notifications::Manager>(std::move(notifications));
        if (options_.storage) {
            container.instance<storage::Manager>(options_.storage);
            container.instance<storage::Service>(std::make_shared<storage::Service>(options_.storage));
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
