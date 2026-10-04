#include "structured_background_program.cpp"
#include <gungnir/language/compiler.hpp>
#include <gungnir/language/cpp_ir.hpp>
#include <gungnir/cache/memory_store.hpp>
#include <gungnir/queue/memory_driver.hpp>
#include <gungnir/production/runtime_host.hpp>
#ifdef GUNGNIR_WITH_SQLITE
#include <gungnir/database/sqlite.hpp>
#endif
#include <coroutine>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <thread>
using namespace gungnir;
using namespace std::chrono_literals;
#define CHECK(...) do { if (!(__VA_ARGS__)) { std::cerr << __FILE__ << ':' << __LINE__ << ": " << #__VA_ARGS__ << std::endl; std::exit(EXIT_FAILURE); } } while(false)
namespace {
template<class F> void fails(F action) { bool caught{}; try { action(); } catch (const std::exception&) { caught=true; } CHECK(caught); }
void reject(String source) {
    const auto emitted=language::Compiler{}.compile(source,"invalid-background.gnr");
    language::CompilerOptions options; options.validate_only=true;
    const auto checked=language::Compiler{}.compile(source,"invalid-background.gnr",options);
    CHECK(!emitted.success() && !checked.success() && emitted.code.empty() && checked.code.empty());
    CHECK(emitted.diagnostics.size()==checked.diagnostics.size());
    for (std::size_t i=0;i<emitted.diagnostics.size();++i) {
        const auto& a=emitted.diagnostics[i]; const auto& b=checked.diagnostics[i];
        CHECK(a.code==b.code && a.message==b.message && a.span.valid && b.span.valid);
        CHECK(a.span.begin_offset==b.span.begin_offset && a.span.end_offset==b.span.end_offset);
    }
}
struct ManualClock : scheduler::Clock { TimePoint value{}; TimePoint now() const override {return value;} };
struct NativeSchedule { ManualClock clock; scheduler::Scheduler scheduler{clock}; };
struct Pause {
    std::coroutine_handle<>* handle;
    bool await_ready() const noexcept {return false;}
    void await_suspend(std::coroutine_handle<> value) const noexcept {*handle=value;}
    void await_resume() const noexcept {}
};
struct TestEvent : events::Event {
    String value; explicit TestEvent(String value) : value(std::move(value)) {}
    std::string_view name() const noexcept override {return "owned";}
};
Task<void> complete(Task<Response> task) { (void)co_await task; }
Task<int> start(Task<void>& task) { co_await task; co_return 0; }
Task<void> paused(const events::Event& value,std::coroutine_handle<>& handle,String& observed) {
    co_await Pause{&handle}; observed=dynamic_cast<const TestEvent&>(value).value;
}
ServiceOptions options(std::shared_ptr<queue::MemoryDriver> driver) {
    ServiceOptions result; result.cache=std::make_shared<cache::MemoryStore>(); result.queue=std::move(driver);
    result.scheduler_locks=std::make_shared<scheduler::MemoryLockStore>();
    result.worker_options.retry_backoff={1ms,2ms}; return result;
}
void listeners(Application& app) {
    register_listener(app,First::make(app.container())); register_listener(app,Second::make(app.container()));
    register_listener(app,AsyncRecord::make(app.container()));
    register_job<RecordJob>(app); register_job<AsyncJob>(app);
}
}
int main() {
    std::cerr << "background: canonical diagnostics\n";
    const String prelude="event E { int id; } job J { inject Cache cache; int id; handle() {} } ";
    for (const auto body : {"events.dispatch(J(1));", "queue.dispatch(E(1));", "queue.dispatch(J(1),0);", "queue.dispatch(J(1),4294967296);", "queue.later(J(1),-1);", "queue.later(J(1),'soon');", "events.dispatchAsync(E(1));", "schedule.every('x',0,J(1));", "schedule.cron('x','bad',J(1));", "schedule.every('x',1,() => 1);", "schedule.every('x',1,(int x) => {});", "schedule.every('x',1,() => {schedule.daily('y',J(1));});", "J(1).handle();", "const service=J(1).cache;", "const service=Queue;", "const service=Queue();", "const service=Events();", "const service=Scheduler();", "const service=ScheduledTask();", "json({service:queue});"})
        reject(prelude+"function void bad(Events events, Queue queue, Scheduler schedule) {"+body+"}");
    reject("event E {int id;} job J {E event; handle() {}}");
    reject("job J {Response response; handle() {}}");
    reject("model M {int value;} job J {Collection<M> values; handle() {}}");
    reject("event E {int id;} listener L { priority='high'; handle(E e) {} }");
    reject("event E {int id;} listener L { priority=2147483648; handle(E e) {} }");
    reject("event E {int id;} listener L { priority=1; priority=2; handle(E e) {} }");
    reject("event E {int id;} listener L { priority=1; int priority; handle(E e) {} }");
    reject("async function void callback() {} function void bad(Scheduler s) {s.daily('x',callback);}");
    reject("function void bad(Queue q, J? j) {q.dispatch(j);} job J {handle(){}}");
    auto valid=language::Compiler{}.compile("event E {int id;} async function void send(Events events) {await events.dispatchAsync(E(1));}","ir.gnr");
    CHECK(valid.success()); CHECK(language::CppIrVerifier{}.verify(language::CppIrLowerer{}.lower(*valid.validated,false)).success());
    auto console=language::SyntaxParser{}.parse("function int schedule(Scheduler s) {return 1;}","routes/console.gnr","routes.console");
    CHECK(!language::ProgramValidator{}.validate(std::move(console.project),{}).project.has_value());

    std::cerr << "background: event injection and ordering\n";
    auto driver=std::make_shared<queue::MemoryDriver>();
    Application app; app.provider<ServicesProvider>(options(driver)); app.boot(); auto context=app.activate(); listeners(app);
    auto events=app.container().resolve<events::Service>(); auto queue=app.container().resolve<queue::Service>();
    auto cache=app.container().resolve<cache::Values>(); auto worker=app.container().resolve<queue::Worker>();
    std::vector<int> order;
    auto native_events=app.container().resolve<events::Dispatcher>();
    (void)native_events->listen(String{Pulse::event_name},[&](const events::Event&){CHECK(cache->get("order")); order.push_back(10);},10);
    (void)native_events->listen(String{Pulse::event_name},[&](const events::Event&){CHECK(cache->get("last")); order.push_back(-20);},-20);
    emit(*events,5); CHECK(order==std::vector<int>({10,-20})); CHECK(cache->get("last")->string()=="5");
    fails([&]{events->dispatch(Completed{"sync"});});
    language::runtime::wait(emitAsync(*events,"async")); CHECK(cache->get("last")->string()=="7");
    auto controller=BackgroundController::make(app.container()); CHECK(!controller->send(4).body().empty());
    CHECK(worker->run_one()); CHECK(cache->get("controller")->string()=="4");
    language::runtime::wait(complete(controller->sendAsync()));

    std::cerr << "background: actual suspension and ownership\n";
    std::coroutine_handle<> resume; String observed;
    std::weak_ptr<events::Dispatcher> dispatcher_owner;
    auto pending=[&] {
        auto dispatcher=std::make_shared<events::Dispatcher>(); dispatcher_owner=dispatcher;
        (void)dispatcher->listen_async("owned",[&](const events::Event& event){return paused(event,resume,observed);});
        events::Service service{dispatcher}; return service.dispatch_async(TestEvent{"temporary"});
    }();
    CHECK(!dispatcher_owner.expired()); auto started=start(pending); started.run_inline(); CHECK(resume && observed.empty());
    resume.resume(); CHECK(observed=="temporary" && pending.done());
    pending=Task<void>{}; CHECK(dispatcher_owner.expired());
    std::weak_ptr<events::Dispatcher> shutdown_owner;
    { Application temporary; temporary.provider<ServicesProvider>(options(std::make_shared<queue::MemoryDriver>())); temporary.boot(); listeners(temporary); shutdown_owner=temporary.container().resolve<events::Dispatcher>(); }
    CHECK(shutdown_owner.expired());

    std::cerr << "background: payload reconstruction, retries and delays\n";
    auto id=enqueue(*queue,"job",21,2,true); CHECK(!id.empty());
    auto envelope=driver->pop(); CHECK(envelope && Json::parse(envelope->payload)==Json::parse(R"({"key":"job","value":21})"));
    CHECK(!RecordJob{"producer",1}.cache); driver->push(*envelope); CHECK(worker->run_one()); CHECK(cache->get("job")->string()=="21");
    queue->dispatch(AsyncJob{"async-job"}); CHECK(worker->run_one()); CHECK(cache->get("last")->string()=="7");
    fails([&]{enqueue(*queue,"bad",1,-1,true);}); fails([&]{enqueueLater(*queue,std::numeric_limits<Int64>::max());});
    enqueueLater(*queue,20); CHECK(!worker->run_one());
    std::this_thread::sleep_for(25ms); CHECK(worker->run_one()); CHECK(cache->get("delayed")->string()=="42");
    int attempts{}; worker->handle(String{RecordJob::event_name},[&](std::string_view){++attempts; throw std::runtime_error("fail");});
    id=enqueue(*queue,"retry",1,2,true); CHECK(worker->run_one()); CHECK(!worker->run_one());
    std::this_thread::sleep_for(3ms); CHECK(worker->run_one()); CHECK(attempts==2);
    CHECK(failures(*queue).as_array().size()==1); CHECK(failures(*queue).as_array()[0].get("payload")==nullptr);
    CHECK(retryJob(*queue,id)); register_job<RecordJob>(app); CHECK(worker->run_one()); CHECK(cache->get("retry"));
    CHECK(!retryJob(*queue,id));
    driver->push({"invalid",String{RecordJob::event_name},"{\"key\":1}",0,1}); CHECK(worker->run_one());
    CHECK(forgetJob(*queue,"invalid") && failures(*queue).as_array().empty());
#ifdef GUNGNIR_WITH_SQLITE
    std::cerr << "background: real database commit and rollback\n";
    database::Settings settings; settings.backend=database::Backend::sqlite; settings.database=":memory:";
    auto connection=std::make_shared<database::Connection>("background",std::make_shared<database::SQLiteDriver>(settings));
    database::runtime::detail::ConnectionScope database_scope{connection};
    connection->begin(); enqueue(*queue,"commit",1,1,true); CHECK(!worker->run_one()); connection->commit(); CHECK(worker->run_one());
    connection->begin(); enqueue(*queue,"rollback",1,1,true); connection->rollback(); CHECK(!worker->run_one()); CHECK(!cache->has("rollback"));
    connection->begin(); enqueue(*queue,"immediate",1,1,false); CHECK(worker->run_one()); connection->rollback(); CHECK(cache->has("immediate"));
#endif
    std::cerr << "background: clock, cron, locks and retained task handles\n";
    auto native=std::make_shared<NativeSchedule>();
    auto scheduler=std::shared_ptr<scheduler::Scheduler>{native,&native->scheduler};
    auto locks=std::make_shared<scheduler::MemoryLockStore>();
    scheduler::Service schedule{scheduler,app.container().resolve<queue::Dispatcher>(),locks};
    auto task=periodic(schedule,1000); calendar(schedule,*cache); namedSchedule(schedule);
    CHECK(scheduler->size()==6); CHECK(scheduler->run_due()==5); CHECK(worker->run_one()); CHECK(cache->get("scheduled")->string()=="9");
    CHECK(cache->get("calendar")); CHECK(worker->run_one()); CHECK(cache->get("daily")); CHECK(!worker->run_one()); CHECK(scheduler->run_due()==0);
    native->clock.value+=1s; CHECK(scheduler->run_due()==1); CHECK(worker->run_one());
    fails([&]{schedule.every("fixed",0,[]{});}); schedule.every("fixed",1,[]{});
    fails([&]{schedule.cron("corrected","bad",[]{});}); schedule.cron("corrected","* * * * *",[]{});
    scheduler::Service no_locks{std::make_shared<scheduler::Scheduler>(native->clock)};
    fails([&]{no_locks.every("needs-lock",1,[]{}).onOneServer();});
    auto other=std::make_shared<NativeSchedule>(); other->clock.value=native->clock.value;
    auto other_scheduler=std::shared_ptr<scheduler::Scheduler>{other,&other->scheduler};
    scheduler::Service other_service{other_scheduler,app.container().resolve<queue::Dispatcher>(),locks};
    periodic(other_service,1000); CHECK(other_scheduler->run_due()==0);
    std::weak_ptr<NativeSchedule> task_owner;
    auto owned_task=[&] { auto owner=std::make_shared<NativeSchedule>(); task_owner=owner;
        scheduler::Service service{std::shared_ptr<scheduler::Scheduler>{owner,&owner->scheduler}};
        return service.daily("retained-owner",[]{}); }();
    CHECK(!task_owner.expired()); owned_task.timezone("UTC");
    auto retained=[&]{scheduler::Service temporary{scheduler}; return temporary.daily("owned",[]{}); }();
    retained.timezone("UTC"); fails([&]{retained.timezone("../bad");});

    std::cerr << "background: native runtime startup and shutdown\n";
    auto running=app.container().resolve<scheduler::Scheduler>();
    auto live=*app.container().resolve<scheduler::Service>();
    live.every("live",10,RecordJob{"live",55});
    production::RuntimeHost host{app,{.shutdown_timeout=1s,.poll_interval=1ms}};
    host.queue(*worker).scheduler(*running,{.maximum_sleep=1ms}); host.start();
    for(int i=0;i<1000 && !cache->has("live");++i) std::this_thread::sleep_for(1ms);
    CHECK(cache->get("live")->string()=="55"); CHECK(worker->running() && running->running());
    fails([&]{live.daily("late",[]{});}); host.request_stop(); CHECK(host.shutdown().graceful);
    CHECK(!worker->running() && !running->running()); CHECK(native_events->listener_count(Pulse::event_name)==0);
    std::cout << "Generated background application contract passed\n";
}
