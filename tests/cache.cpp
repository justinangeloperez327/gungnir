#include <cassert>
#include <chrono>
#include <string>
#include <atomic>
#include <cmath>
#include <barrier>
#include <iostream>
#include <limits>
#include <thread>
#include <vector>

#include <gungnir/cache/cache.hpp>

#define CHECK(...) do { if (!(__VA_ARGS__)) { std::cerr << __LINE__ << ": " << #__VA_ARGS__ << '\n'; std::abort(); } } while (false)

namespace {
struct LosingStore final : gungnir::cache::LockStore {
    int releases = 0;
    std::optional<gungnir::scheduler::LockLease> acquire(std::string key, std::chrono::milliseconds) override {
        return gungnir::scheduler::LockLease{std::move(key), "owner"};
    }
    bool renew(const gungnir::scheduler::LockLease&, std::chrono::milliseconds) override { return false; }
    bool release(const gungnir::scheduler::LockLease&) override { ++releases; return false; }
};
struct FillingStore final : gungnir::cache::LockStore {
    gungnir::cache::Store& values;
    gungnir::cache::MemoryLockStore locks;
    explicit FillingStore(gungnir::cache::Store& values) : values(values) {}
    std::optional<gungnir::scheduler::LockLease> acquire(std::string key, std::chrono::milliseconds ttl) override {
        values.put("filled-race", "winner");
        return locks.acquire(std::move(key), ttl);
    }
    bool renew(const gungnir::scheduler::LockLease& lease, std::chrono::milliseconds ttl) override { return locks.renew(lease, ttl); }
    bool release(const gungnir::scheduler::LockLease& lease) override { return locks.release(lease); }
};
}

int main() {
    using namespace gungnir;

    cache::MemoryStore store;
    cache::Repository cache{store};

    assert(!cache.get("missing"));
    cache.put("name", "Gungnir");
    assert(cache.has("name"));
    assert(cache.get("name").value() == "Gungnir");

    int calls = 0;
    const auto first = cache.remember(
        "computed",
        std::chrono::seconds{60},
        [&] {
            ++calls;
            return std::string{"value"};
        }
    );
    const auto second = cache.remember(
        "computed",
        std::chrono::seconds{60},
        [&] {
            ++calls;
            return std::string{"other"};
        }
    );
    assert(first == "value");
    assert(second == "value");
    assert(calls == 1);

    assert(cache.forget("name"));
    assert(!cache.has("name"));

    cache.put("temporary", "value", std::chrono::seconds{0});
    assert(!cache.get("temporary"));

    cache.put("a", "1");
    cache.flush();
    assert(!cache.has("a"));

    auto locks = std::make_shared<cache::MemoryLockStore>();
    cache::Repository coordinated{store, locks};
    auto values = cache::Values{std::make_shared<cache::Repository>(store, locks)};
    {
        auto first = values.lock("shared", 5000);
        CHECK(first.acquire() && !first.acquire());
        auto copy = first;
        auto second = values.lock("shared", 5000);
        CHECK(!second.acquire());
        values.flush();
        CHECK(!second.acquire()); // A value flush never removes coordination leases.
        CHECK(copy.renew() && copy.release() && !first.release());
        CHECK(second.acquire());
    }
    CHECK(!locks->locked("shared"));
    try {
        auto lease = values.lock("unwind", 5000);
        CHECK(lease.acquire());
        throw std::runtime_error("failed action");
    } catch (const std::runtime_error&) {}
    CHECK(!locks->locked("unwind"));
    auto expired = locks->acquire("expired", std::chrono::milliseconds{1}); CHECK(expired);
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
    CHECK(!locks->release(*expired));
    for (const auto ttl : {gungnir::Int64{0}, gungnir::Int64{-1}, std::numeric_limits<gungnir::Int64>::max()}) {
        bool rejected = false;
        try { (void)values.lock("bad", ttl); } catch (const std::invalid_argument&) { rejected = true; }
        CHECK(rejected && !locks->locked("bad"));
    }
    {
        std::weak_ptr<cache::LockStore> owner;
        std::optional<cache::Lock> held;
        {
            auto backend = std::make_shared<cache::MemoryLockStore>(); owner = backend;
            held.emplace(backend, "retained", std::chrono::seconds{5});
            CHECK(held->acquire());
        }
        CHECK(!owner.expired() && held->renew());
        held.reset(); CHECK(owner.expired());
    }
    int locked_calls = 0;
    CHECK(values.rememberLocked("computed", 60, 5000, [&] { ++locked_calls; return "shared"; }).string() == "shared");
    CHECK(values.rememberLocked("computed", 60, 5000, [&] { ++locked_calls; return "other"; }).string() == "shared");
    CHECK(locked_calls == 1 && !locks->locked("remember:computed"));
    {
        auto held = values.lock("remember:contended", 5000); CHECK(held.acquire());
        bool rejected = false;
        try { (void)values.rememberLocked("contended", 60, 5000, [&] { ++locked_calls; return "bad"; }); }
        catch (const cache::LockUnavailable&) { rejected = true; }
        CHECK(rejected && locked_calls == 1 && !values.has("contended"));
    }
    bool failed = false;
    try { (void)values.rememberLocked("failure", 60, 5000, []() -> String { throw std::runtime_error("failure"); }); }
    catch (const std::runtime_error&) { failed = true; }
    CHECK(failed && !values.has("failure") && !locks->locked("remember:failure"));
    bool invalid_value = false;
    try { (void)values.rememberLocked("invalid-json", 60, 5000, [] { return std::nan(""); }); }
    catch (const std::invalid_argument&) { invalid_value = true; }
    CHECK(invalid_value && !values.has("invalid-json") && !locks->locked("remember:invalid-json"));
    auto filling = std::make_shared<FillingStore>(store);
    cache::Repository race{store, filling};
    CHECK(race.remember_locked("filled-race", std::chrono::seconds{60}, std::chrono::seconds{5}, [&] {
        ++locked_calls; return "loser";
    }) == "winner");
    CHECK(locked_calls == 1 && !filling->locks.locked("remember:filled-race"));
    auto lost = std::make_shared<LosingStore>();
    cache::Repository losing{store, lost};
    bool lost_lease = false;
    try { (void)losing.remember_locked("lost", std::chrono::seconds{60}, std::chrono::seconds{5}, [] { return "late"; }); }
    catch (const cache::LockLost&) { lost_lease = true; }
    CHECK(lost_lease && !store.has("lost"));

    // Simultaneous callers either win, see contention, or observe the filled value.
    std::barrier start{8};
    std::atomic<int> computations{};
    std::vector<std::thread> contenders;
    for (int i = 0; i < 8; ++i) contenders.emplace_back([&] {
        start.arrive_and_wait();
        try {
            CHECK(values.rememberLocked("concurrent", 60, 5000, [&] {
                ++computations; return String{"one"};
            }).string() == "one");
        } catch (const cache::LockUnavailable&) {}
    });
    for (auto& contender : contenders) contender.join();
    CHECK(computations == 1 && values.get("concurrent")->string() == "one");

    return 0;
}
