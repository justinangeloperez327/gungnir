#include "structured_program.cpp"
#include "structured_modules.cpp"
#include <gungnir/queue/memory_driver.hpp>
#include <gungnir/mail/memory_transport.hpp>
#include <gungnir/testing/http.hpp>
#include <cassert>
#include <utility>
int main() {
    gungnir::Request request{gungnir::http::Method::get,"/"}; request.set_header("X-Name","Ada");
    Home home; assert(home.index(request).body() == "Ada"); Helpers helpers; assert(helpers.plan_up().empty());
    PassThrough pass_through;
    gungnir::Next next = [](gungnir::Request&) -> gungnir::Task<gungnir::Response> {
        co_return gungnir::Response::text("next");
    };
    auto middleware_task = pass_through.handle(request,std::move(next));
    middleware_task.run_inline();
    assert(middleware_task.operator co_await().await_resume().body() == "next");
    assert(AuditRecord::uses_timestamps());
    assert(AuditRecord::uses_soft_deletes());
    assert(AuditRecord::has_attribute("created_at"));
    assert(AuditRecord::has_attribute("updated_at"));
    assert(AuditRecord::has_attribute("deleted_at"));
    assert(AuditRecord::deleted_at_column() == std::string_view{"deleted_at"});
    assert(gnr::main::result() == 11);
    assert(floating() == 3.5 && minimum() == std::numeric_limits<gungnir::Int64>::min());
    assert(embedded().size() == 3 && embedded()[1] == '\0' && lookup().string() == "hello" && loop() == 3);
    assert(add(5) == 7 && named() == 10 && greeting("Ada") == "Hello Ada");
    assert(doubled() == std::vector<gungnir::Int64>({2,4,6}));
    assert(nullable(std::nullopt) == 9 && nullable(4) == 4);
    assert(closure(6) == 10 && unicode() == "AΩ" && separators() == 1042);
    auto task = twice(4); task.run_inline(); assert(task.done()); assert(task.operator co_await().await_resume() == 10);
    auto nested_task = nested(5); nested_task.run_inline(); assert(nested_task.operator co_await().await_resume() == 8);
    assert(absent().get("item")->is_null() && !emptyOptional());
    Created event{5,"new"}; assert(event.name() == "Created");
    gungnir::events::Dispatcher dispatcher; RecordCreated::register_listener(dispatcher,std::make_shared<RecordCreated>()); dispatcher.dispatch(event); assert(dispatcher.listener_count("Created") == 1);
    assert(guarded(std::nullopt) == 0 && guarded(4) == 5);
    gungnir::events::Dispatcher async_dispatcher; AsyncRecord::register_listener(async_dispatcher,std::make_shared<AsyncRecord>()); gungnir::language::runtime::wait(async_dispatcher.dispatch_async(event));
    bool async_rejected = false; try { async_dispatcher.dispatch(event); } catch (const std::logic_error&) { async_rejected = true; } assert(async_rejected);
    gungnir::queue::MemoryDriver driver; gungnir::queue::Worker worker{driver}; AsyncPing::register_job(worker); AsyncPing async_job{12}; driver.push({"1",std::string{async_job.name()},async_job.payload()}); assert(worker.run_one());
    gungnir::Container container; auto injected = InjectedPing::make(container,4); InjectedPing::register_job(worker,container); driver.push({"2",std::string{injected->name()},injected->payload()}); assert(worker.run_one()); assert(eventDefault() == 1);
    Ping job{7}; assert(Ping::from_payload(job.payload()).id == 7);
    bool rejected = false; try { Ping::from_payload("{\"id\":\"wrong\"}"); } catch (const std::invalid_argument&) { rejected = true; } assert(rejected);
    Welcome mail{"Ada"}; assert(mail.message().subject_line() == "Welcome Ada");
    CreateUsers migration; assert(!migration.plan_up().empty() && !migration.plan_down().empty());
    User user; user.id = 8; User other; other.id = 9;
    gungnir::auth::ResourceAuthorization authorization; UserPolicy::register_policy(authorization,std::make_shared<UserPolicy>()); assert(authorization.inspect("view",user,user).allowed); assert(!authorization.inspect("view",user,other).allowed);
    Greeting notification{"Hello"}; auto bound = notification.bind(user); assert(bound.channels().size() == 2); assert(bound.to_database().get("title")->string() == "Hello");
    assert(User::hidden[0] == "secret" && User::casts[0].first == "active");
    static_assert(std::tuple_size_v<decltype(gungnir::model::Generated<User>::attributes)> == 4);
    gungnir::Request payload{gungnir::http::Method::post,"/",R"({"enabled":false,"count":4,"extra":true})"};
    payload.set_header("Content-Type","application/json");
    auto validated=validatePayload(payload);
    assert(validated.get("enabled")->is_boolean() && validated.get("count")->is_integer() && !validated.get("extra"));
    const auto precise = exactAmount();
    assert(precise.string() == "12345678901234567890.123400");
    auto price = Price::hydrate({{"amount",precise}});
    assert(price.amount.get() == precise);
    auto queue = std::make_shared<gungnir::queue::MemoryDriver>();
    auto transport = std::make_shared<gungnir::mail::MemoryTransport>();
    gungnir::Application app;
    app.provider<gungnir::ServicesProvider>(gungnir::ServiceOptions{
        .queue=queue,.mail=transport,.sender={"sender@example.com","Gungnir"}});
    app.on_boot([&](gungnir::Application& application) {
        gungnir::register_job<Ping>(application);
        gungnir::register_job<InjectedPing>(application);
        gungnir::register_listener<RecordCreated>(application,std::make_shared<RecordCreated>());
        gungnir::register_policy<UserPolicy>(application,std::make_shared<UserPolicy>());
    });
    app.boot();
    auto resources=app.container().resolve<gungnir::auth::ResourceAuthorization>();
    resources->actor<User>([user](const gungnir::auth::Identity&) { return std::optional{user}; });
    app.router().use(gungnir::session::middleware(std::make_shared<gungnir::session::MemoryStore>()));
    app.router().use(gungnir::auth::session([](std::string_view id) -> std::optional<gungnir::auth::Identity> { return gungnir::auth::Identity{.id=std::string{id}}; }));
    app.router().get("/policy",[&](gungnir::Request& current) -> gungnir::Task<gungnir::Response> {
        current.auth().login(gungnir::auth::Identity{.id="8"});
        checkUser(current,user);
        bool forbidden=false;
        try { checkUser(current,other); } catch (const gungnir::auth::AuthorizationError&) { forbidden=true; }
        assert(forbidden);
        co_return gungnir::Response::text("authorized");
    });
    gungnir::testing::Http client{app.router()};
    assert(client.get("/policy").body()=="authorized");
    app.container().resolve<gungnir::queue::Dispatcher>()->dispatch(Ping{4},1,false);
    assert(app.container().resolve<gungnir::queue::Worker>()->run_one());
    app.container().resolve<gungnir::events::Dispatcher>()->dispatch(event);
    app.container().resolve<gungnir::notifications::Manager>()->send("user@example.com",WelcomeNotice{}.bind(user));
    assert(transport->messages().size()==1 && transport->messages()[0].subject_line()=="Welcome Ada");
    auto detached_queue=std::make_shared<gungnir::queue::MemoryDriver>();
    std::shared_ptr<gungnir::queue::Worker> retained_worker;
    {
        gungnir::Application temporary;
        temporary.provider<gungnir::ServicesProvider>(gungnir::ServiceOptions{.queue=detached_queue});
        temporary.boot();
        gungnir::register_job<InjectedPing>(temporary);
        retained_worker=temporary.container().resolve<gungnir::queue::Worker>();
    }
    detached_queue->push({"expired","InjectedPing","{\"id\":4}"});
    assert(retained_worker->run_one() && detached_queue->failed()==1);

}
