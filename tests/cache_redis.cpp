#include <gungnir/cache/cache.hpp>
#include <gungnir/http/redis_rate_limit.hpp>
#include <gungnir/redis/client.hpp>
#include <gungnir/security/random.hpp>

#include <atomic>
#include <barrier>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

#define CHECK(...) do { if (!(__VA_ARGS__)) { std::cerr << __LINE__ << ": " << #__VA_ARGS__ << '\n'; std::abort(); } } while (false)
using namespace gungnir;
using namespace std::chrono_literals;

int main() {
    const auto host = std::getenv("GUNGNIR_REDIS_HOST") ? std::getenv("GUNGNIR_REDIS_HOST") : "127.0.0.1";
    const auto port = static_cast<std::uint16_t>(std::stoi(std::getenv("GUNGNIR_REDIS_PORT") ? std::getenv("GUNGNIR_REDIS_PORT") : "6379"));
    const auto prefix = "gungnir:coordination:" + security::random_token(8) + ":";
    redis::ClientOptions client_options;
    client_options.nodes = {{host, port}}; client_options.database = 11;
    redis::Client client{client_options};
    CHECK(client.ping());
    cache::RedisSettings settings;
    settings.host = host; settings.port = port; settings.database = 11; settings.prefix = prefix + "values:";
    auto store = std::make_shared<cache::RedisStore>(settings);
    cache::RedisLockSettings lease_settings;
    lease_settings.host = host; lease_settings.port = port; lease_settings.database = 11;
    lease_settings.prefix = prefix + "locks:";
    auto locks = std::make_shared<cache::RedisLockStore>(lease_settings);
    auto other = std::make_shared<cache::RedisLockStore>(lease_settings);
    cache::Values values{std::make_shared<cache::Repository>(*store, locks)};

    auto old = locks->acquire("stale", 60ms); CHECK(old);
    std::this_thread::sleep_for(100ms);
    auto replacement = other->acquire("stale", 5s); CHECK(replacement);
    CHECK(!locks->release(*old) && !locks->renew(*old, 5s));
    CHECK(!locks->acquire("stale", 5s) && other->release(*replacement));
    auto first = values.lock("held", 5000); CHECK(first.acquire());
    values.flush(); CHECK(!other->acquire("held", 5s));
    CHECK(first.renew(5000) && first.release());

    int calls = 0;
    CHECK(values.rememberLocked("null", 60, 5000, [&] { ++calls; return nullptr; }).is_null());
    CHECK(values.rememberLocked("null", 60, 5000, [&] { ++calls; return 1; }).is_null());
    CHECK(calls == 1 && values.has("null"));
    bool expired = false;
    try {
        (void)values.rememberLocked("late", 60, 10, [] {
            std::this_thread::sleep_for(40ms); return String{"late"};
        });
    } catch (const cache::LockLost&) { expired = true; }
    CHECK(expired && !values.has("late"));

    // Flush must use literal prefixes even when they contain Redis glob bytes.
    lease_settings.prefix = prefix + "[x]*?:";
    cache::RedisLockStore literal{lease_settings};
    auto own = literal.acquire("own", 5s); CHECK(own);
    const auto outside = prefix + "x-other:";
    CHECK(client.command({"SET", outside, "other", "PX", "5000"}).ok());
    literal.flush();
    CHECK(!literal.renew(*own, 5s));
    CHECK(client.command({"GET", outside}).text == "other");

    http::RedisRateLimitSettings rate_settings;
    rate_settings.client = client_options; rate_settings.prefix = prefix + "rate:";
    http::RedisRateLimitStore rate{rate_settings}, other_rate{rate_settings};
    CHECK(rate.ping());
    std::barrier start{8};
    std::atomic<int> admitted{};
    std::vector<std::thread> consumers;
    for (int i = 0; i < 8; ++i) consumers.emplace_back([&, i] {
        start.arrive_and_wait();
        for (int attempt = 0; attempt < 25; ++attempt) {
            const auto decision = (i % 2 ? rate : other_rate).consume("shared", 17, 60s);
            if (decision.allowed) ++admitted;
            CHECK(decision.remaining <= 16 && decision.retry_after > 0s && decision.retry_after <= 60s);
        }
    });
    for (auto& consumer : consumers) consumer.join();
    CHECK(admitted == 17);
    CHECK(rate.consume("different-client", 1, 60s).allowed);
    auto isolated_settings = rate_settings; isolated_settings.prefix = prefix + "isolated:";
    http::RedisRateLimitStore isolated{isolated_settings};
    CHECK(isolated.consume("shared", 1, 60s).allowed);
    CHECK(rate.consume("expires", 1, 1s).allowed);
    std::this_thread::sleep_for(200ms);
    const auto before = client.command({"PTTL", rate_settings.prefix + "expires"}).integer;
    CHECK(!other_rate.consume("expires", 1, 1s).allowed);
    const auto after = client.command({"PTTL", rate_settings.prefix + "expires"}).integer;
    CHECK(after > 0 && after <= before && after < 950);
    std::this_thread::sleep_for(1100ms);
    CHECK(other_rate.consume("expires", 1, 1s).allowed);
    CHECK(client.command({"SET", rate_settings.prefix + "corrupt", "not-a-counter", "EX", "5"}).ok());
    bool corrupt = false;
    try { (void)rate.consume("corrupt", 1, 60s); } catch (const std::runtime_error&) { corrupt = true; }
    CHECK(corrupt);
    auto unavailable_settings = rate_settings;
    unavailable_settings.client.nodes = {{"127.0.0.1", 1}};
    unavailable_settings.client.connect_timeout = 50ms;
    http::RedisRateLimitStore unavailable{unavailable_settings};
    bool failed = false;
    try { (void)unavailable.consume("failure", 1, 60s); } catch (const std::runtime_error&) { failed = true; }
    CHECK(failed);

    store->flush(); locks->flush();
    // Isolated literal cleanup; never flush an entire database.
    std::string cursor{"0"};
    do {
        const auto reply = client.command({"SCAN", cursor, "COUNT", "256"});
        cursor = reply.elements.at(0).text;
        for (const auto& key : reply.elements.at(1).elements)
            if (key.text.starts_with(prefix)) (void)client.command({"DEL", key.text});
    } while (cursor != "0");
    std::cout << "Redis lease ownership, literal namespaces and atomic shared quotas passed\n";
}
