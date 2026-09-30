#include <gungnir/language/runtime.hpp>
#include <gungnir/language/compiler.hpp>
#include <cassert>
#include <thread>
#include <chrono>
struct ResumeOnThread {
    std::thread& worker;
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> continuation) { worker = std::thread{[continuation] { std::this_thread::sleep_for(std::chrono::milliseconds{2}); continuation.resume(); }}; }
    void await_resume() const noexcept {}
};
gungnir::Task<void> finish(std::thread& worker, bool& completed) { co_await ResumeOnThread{worker}; completed = true; }
gungnir::Task<void> fail() { throw std::runtime_error("expected"); co_return; }
int main() {
    gungnir::auth::ResourceAuthorization authorization;
    authorization.define<int,int>("view", [](int actor,int resource) { return actor == resource; });
    authorization.define<int,std::string>("view", [](int actor,const std::string& resource) { return actor == 1 && resource == "allowed"; });
    assert(authorization.inspect("view",1,1).allowed);
    assert(authorization.inspect("view",1,std::string{"allowed"}).allowed);
    assert(!authorization.inspect("view",1,2).allowed);
    assert(!authorization.inspect("view",std::string{"wrong"},1).allowed);
    std::thread worker; bool completed = false; gungnir::language::runtime::wait(finish(worker,completed)); worker.join(); assert(completed);
    bool failed = false; try { gungnir::language::runtime::wait(fail()); } catch (const std::runtime_error&) { failed = true; } assert(failed);
    static_assert(!std::is_default_constructible_v<gungnir::language::ValidatedProject>);
}
