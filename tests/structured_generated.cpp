#include "structured_program.cpp"
#include "structured_modules.cpp"
#include <gungnir/queue/memory_driver.hpp>
#include <cassert>
int main() {
    gungnir::Request request{gungnir::http::Method::get,"/"}; request.set_header("X-Name","Ada");
    Home home; assert(home.index(request).body() == "Ada"); Helpers helpers; assert(helpers.plan_up().empty());
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
}
