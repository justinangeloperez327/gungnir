#include <cassert>
#include <chrono>
#include <string>

#include <gungnir/cache/cache.hpp>

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

    return 0;
}
