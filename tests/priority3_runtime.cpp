#include <gungnir/core/application.hpp>
#include <gungnir/core/task_group.hpp>
#include <gungnir/core/executor.hpp>
#include <gungnir/core/timer.hpp>
#include <gungnir/testing/http.hpp>
#include <gungnir/database/runtime.hpp>
#include <gungnir/view/runtime.hpp>
#include <gungnir/routing/route.hpp>
#include <cassert>
#include <atomic>
#include <latch>
#include <thread>

using namespace gungnir;
using namespace std::chrono_literals;
struct ProbeProvider : Provider {
    std::vector<int>& calls;
    int id;
    int fail;
    ProbeProvider(std::vector<int>& calls,int id,int fail) : calls(calls),id(id),fail(fail) {}
    void register_services(Application&) override { calls.push_back(id); if (fail==1) throw std::runtime_error("registration"); }
    void boot(Application&) override { calls.push_back(id+10); if (fail==2) throw std::runtime_error("boot"); }
    void ready(Application&) override { calls.push_back(id+20); if (fail==3) throw std::runtime_error("ready"); }
    void shutdown(Application&) override { calls.push_back(id+30); if (fail==4) throw std::runtime_error("shutdown"); }
};
Task<int> abandoned_timer(std::atomic<int>& count) { co_await sleep_for(30ms); ++count; co_return 1; }
Task<int> abandoned_executor(Executor& executor,std::atomic<int>& count) { co_await executor.yield(); ++count; co_return 1; }
int main() {
    for (int stage=1;stage<=3;++stage) {
        std::vector<int> calls; Application app;
        app.provider<ProbeProvider>(calls,1,0).provider<ProbeProvider>(calls,2,stage);
        bool failed=false; try { app.boot(); } catch (const std::runtime_error&) { failed=true; }
        assert(failed && !app.is_booted() && app.lifecycle_stage()==LifecycleStage::stopped);
        assert(calls[calls.size()-2]==32 && calls.back()==31);
        auto size=calls.size(); app.shutdown(); assert(calls.size()==size);
    }
    {
        std::vector<int> calls; Application app;
        app.provider<ProbeProvider>(calls,1,4).provider<ProbeProvider>(calls,2,0);
        app.on_shutdown([&](Application&) { calls.push_back(100); });
        app.on_shutdown([](Application&) { throw std::runtime_error("hook"); });
        app.boot(); app.shutdown();
        assert(calls.back()==100 && app.shutdown_errors().size()==2);
    }
    {
        Application first; auto* first_router=&first.router();
        {
            Application second;
            assert(&routing::detail::route_router()==&second.router());
            auto scope=first.activate();
            assert(&routing::detail::route_router()==first_router);
            assert(&database::runtime::manager()==&first.database());
            assert(view::runtime::engine().get()==&first.views());
        }
        assert(&routing::detail::route_router()==first_router);
        Application second;
        first.router().get("/context",[&](http::Request&) -> Task<http::Response> {
            co_await sleep_for(2ms);
            assert(&database::runtime::manager()==&first.database());
            assert(view::runtime::engine().get()==&first.views());
            co_return http::Response::text("first");
        });
        testing::Http client{first.router()};
        assert(client.get("/context").body()=="first");
        Application moved{std::move(first)};
        assert(&moved.router()==first_router);
    }
    {
        std::atomic<int> resumed{0};
        { auto task=abandoned_timer(resumed); task.run_inline(); }
        Executor executor{1,2};
        { auto task=abandoned_executor(executor,resumed); task.run_inline(); }
        executor.start(); executor.stop(); executor.join();
        std::this_thread::sleep_for(50ms); assert(resumed==0);
    }
    {
        Executor executor{1,1}; executor.post([] {});
        bool full=false; try { executor.post([] {}); } catch (const std::length_error&) { full=true; }
        assert(full); executor.start(); executor.stop(); executor.join();
        Executor failures{1}; failures.post([] { throw std::runtime_error("background"); });
        failures.start(); failures.stop(); failures.join(); assert(failures.failure());
    }
    {
        CancellationSource source; std::atomic<bool> cancelled{false};
        TaskGroup group;
        group.spawn([&](CancellationToken token) -> Task<void> {
            try { co_await sleep_for(5s,token); } catch (const OperationCancelled&) { cancelled=true; }
        });
        group.cancel(); group.join(); assert(cancelled);
    }
    {
        TaskGroup group; std::latch started{2}; std::atomic<bool> sibling_cancelled{false};
        group.spawn([&](CancellationToken) -> Task<void> { started.count_down(); started.wait(); throw std::runtime_error("child"); co_return; });
        group.spawn([&](CancellationToken token) -> Task<void> { started.count_down(); started.wait(); try { co_await sleep_for(5s,token); } catch (const OperationCancelled&) { sibling_cancelled=true; } });
        bool failed=false; try { group.join(); } catch (const std::runtime_error&) { failed=true; }
        assert(failed && sibling_cancelled);
    }
}
