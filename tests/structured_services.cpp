#include "structured_services_program.cpp"
#include <gungnir/language/compiler.hpp>
#include <gungnir/cache/memory_store.hpp>
#include <gungnir/storage/local_disk.hpp>

#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <limits>
#include <locale>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

using namespace gungnir;
namespace {
void reject(std::string_view body, std::string_view code = "GNR2201") {
    const auto source = "controller Invalid { inject Cache cache; inject Storage storage; index(Request request) { " + String{body} + " } }";
    const auto emitted = language::Compiler{}.compile(source, "services-invalid.gnr");
    language::CompilerOptions options; options.validate_only = true;
    const auto checked = language::Compiler{}.compile(source, "services-invalid.gnr", options);
    assert(!emitted.success() && !checked.success() && emitted.code.empty() && checked.code.empty());
    assert(emitted.diagnostics.size() == checked.diagnostics.size());
    bool found = false;
    for (std::size_t i = 0; i < emitted.diagnostics.size(); ++i) {
        const auto& a = emitted.diagnostics[i]; const auto& b = checked.diagnostics[i];
        assert(a.code == b.code && a.message == b.message);
        assert(a.location.line == b.location.line && a.location.column == b.location.column);
        assert(a.span.begin_offset == b.span.begin_offset && a.span.end_offset == b.span.end_offset);
        found |= a.code == code;
    }
    assert(found);
}
class Store final : public cache::Store {
    cache::MemoryStore memory_;
public:
    std::optional<cache::Duration> last_ttl;
    int puts = 0;
    std::optional<String> get(std::string_view key) override { return memory_.get(key); }
    void put(String key, String value, std::optional<cache::Duration> ttl = {}) override {
        last_ttl = ttl; ++puts; memory_.put(std::move(key), std::move(value), ttl);
    }
    bool forget(std::string_view key) override { return memory_.forget(key); }
    void flush() override { memory_.flush(); }
};
struct Files {
    std::filesystem::path root = std::filesystem::temp_directory_path() /
        ("gungnir-services-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Files() { assert(std::filesystem::create_directory(root)); }
    ~Files() { std::error_code error; std::filesystem::remove_all(root, error); }
};
struct DecimalComma final : std::numpunct<char> {
    char do_decimal_point() const override { return ','; }
};
Task<void> dispatch(Router& router, Request& request, std::optional<Response>& result) {
    result = co_await router.dispatch(request);
}
Response send(Router& router, String path, String body = {}, bool json_body = false) {
    Request request{body.empty() ? http::Method::get : http::Method::post, std::move(path), std::move(body)};
    request.set_header("Content-Type", json_body ? "application/json" : "application/octet-stream");
    std::optional<Response> result;
    language::runtime::wait(dispatch(router, request, result));
    assert(result); return std::move(*result);
}
}

int main() {
    reject("return json(cache.get());", "GNR2209");
    reject("cache.put(1, {}); return noContent();");
    reject("cache.put(\"k\", {}, true); return noContent();");
    reject("return json(cache.remember(\"k\", 300, []));");
    reject("return json(cache.remember(\"k\", 300, (int value) => value));");
    reject("return json(cache.remember(\"k\", 300, () => cache));");
    reject("return json(cache.remember(\"k\", 300, () => { return () => 1; }));");
    reject("return text(cache.get(\"k\").dump());");
    reject("return json({\"service\": storage.disk()});");
    reject("cache.put(\"k\", [cache]); return noContent();");
    reject("return json(cache);");
    reject("cache.put(\"k\", request); return noContent();");
    reject("cache.put(\"k\", noContent()); return noContent();");
    reject("cache.put(\"k\", {\"request\": request}); return noContent();");
    reject("cache.put(\"k\", [1, request]); return noContent();");
    reject("return json(cache.remember(\"k\", 300, () => request));");
    reject("return json(() => 1);");
    reject("storage.put(\"file\", {}); return noContent();");
    reject("return text(storage.disk(\"archive\").get());", "GNR2209");
    reject("Storage::put(\"file\", \"contents\"); return noContent();");
    const auto leak = language::Compiler{}.compile("job Invalid { Cache cache; handle() {} }", "field.gnr");
    assert(!leak.success() && leak.code.empty());
    const auto model_service = language::Compiler{}.compile("model Invalid { inject Cache cache; }", "model-service.gnr");
    assert(!model_service.success() && model_service.code.empty());
    const auto async_factory = language::Compiler{}.compile(
        "async function Json factory() { return {}; } controller Invalid { inject Cache cache; index(Request request) { return json(cache.remember(\"k\", 300, factory)); } }", "async-factory.gnr");
    assert(!async_factory.success() && async_factory.code.empty());

    Files files;
    auto store = std::make_shared<Store>();
    auto local = std::make_shared<storage::LocalDisk>(files.root / "local");
    auto archive = std::make_shared<storage::LocalDisk>(files.root / "archive");
    auto manager = std::make_shared<storage::Manager>();
    manager->add("local", local).add("archive", archive);
    Application app;
    app.provider<ServicesProvider>(ServiceOptions{.cache=store, .storage=manager});
    auto reports = std::make_shared<ReportSource>(0);
    app.container().instance<ReportSource>(reports);
    app.boot();
    auto controller = ServiceController::make(app.container());
    auto trace = CacheTrace::make(app.container());
    app.router().use([trace](Request& request, Next next) -> Task<Response> {
        co_return co_await trace->handle(request, std::move(next));
    });
    app.router().post("/cache", [controller](Request& r) { return controller->store(r); });
    app.router().get("/cache", [controller](Request& r) { return controller->show(r); });
    app.router().get("/remember", [controller](Request& r) { return controller->remember(r); });
    app.router().get("/named", [controller](Request& r) { return controller->named(r); });
    app.router().get("/null", [controller](Request& r) { return controller->nullValue(r); });
    app.router().get("/expired", [controller](Request& r) { return controller->expired(r); });
    app.router().get("/forget", [controller](Request& r) { return controller->forget(r); });
    app.router().get("/clear", [controller](Request& r) { return controller->clear(r); });
    app.router().post("/file", [controller](Request& r) { return controller->fileStore(r); });
    app.router().get("/file", [controller](Request& r) { return controller->fileShow(r); });
    app.router().post("/archive", [controller](Request& r) { return controller->archive(r); });
    app.router().get("/operations", [controller](Request& r) { return controller->fileOperations(r); });
    assert(send(app.router(), "/cache").status() == 204);
    const String payload = R"({"integer":42,"floating":3.0,"flag":false,"null":null,"list":[1,"two"],"object":{"label":"value"}})";
    assert(send(app.router(), "/cache", payload, true).status() == 204);
    const auto response = send(app.router(), "/cache");
    assert(Json::parse(response.body()) == Json::parse(payload));
    assert(Json::parse(response.body()).get("floating")->is_number());
    assert(!Json::parse(response.body()).get("floating")->is_integer());
    const auto cold = send(app.router(), "/remember");
    const auto warm = send(app.router(), "/remember");
    assert(cold.body() == warm.body() && cold.header("X-Cache") == "warm");
    assert(reports->calls == 1 && controller->cache->get("nested")->as_array().size() == 2);
    const auto middleware = controller->cache->get("middleware");
    assert(middleware->get("before")->dump() == "true" && middleware->get("after")->dump() == "true");
    assert(Json::parse(send(app.router(), "/named").body()).get("kind")->string() == "named");
    assert(send(app.router(), "/null").body() == "true");
    assert(controller->cache->get("null") && controller->cache->get("null")->is_null());
    assert(!controller->cache->get("missing"));
    assert(send(app.router(), "/expired").body() == "false");
    assert(send(app.router(), "/forget").body() == "true" && send(app.router(), "/forget").body() == "false");
    cacheLifetime(*controller->cache, 300);
    assert(store->last_ttl && store->last_ttl->count() == 300);
    const auto writes = store->puts;
    for (const auto seconds : {Int64{-1}, std::numeric_limits<Int64>::max()}) {
        bool rejected = false;
        try { cacheLifetime(*controller->cache, seconds); } catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected && store->puts == writes);
    }
    const auto maximum = std::numeric_limits<UInt64>::max();
    assert(cacheUnsigned(*controller->cache, maximum).string() == std::to_string(maximum));
    assert(cacheUnsigned(*controller->cache, maximum).is_integer());
    assert(cacheFloating(*controller->cache, 3.0).dump() == "3.0");
    assert(cacheFloating(*controller->cache, -0.0).dump() == "-0.0");
    {
        const auto previous = std::locale();
        std::locale::global(std::locale{previous, new DecimalComma});
        assert(cacheFloating(*controller->cache, 1.5).dump() == "1.5");
        std::locale::global(previous);
    }
    assert(cacheFloating(*controller->cache, -0.0).dump() == "-0.0");
    for (const auto value : {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        bool rejected = false;
        try { (void)cacheFloating(*controller->cache, value); } catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected && controller->cache->get("floating")->dump() == "-0.0");
    }
    assert(cacheOptional(*controller->cache, std::nullopt).is_null());
    assert(cacheOptional(*controller->cache, String{"optional"}).string() == "optional");
    const auto profile = CachedProfile::hydrate({{"id", Int64{7}}, {"name", String{"Freya"}}, {"secret", String{"credential"}}});
    const auto visible = cacheProfile(*controller->cache, profile);
    assert(visible.get("name")->string() == "Freya" && !visible.get("secret"));
    assert(cacheOptionalProfile(*controller->cache, std::nullopt).is_null());
    const auto optional_profile = cacheOptionalProfile(*controller->cache, profile);
    assert(optional_profile == visible && !optional_profile.get("secret"));
    const std::unordered_map<String,std::vector<std::optional<Int64>>> nested{{"items", {Int64{1},std::nullopt}}};
    assert(cacheNested(*controller->cache, nested) == Json::parse(R"({"items":[1,null]})"));
    assert(cacheNested(*controller->cache, std::nullopt).is_null());
    bool failed_factory = false;
    try { (void)controller->cache->remember("failure", 300, []() -> Json { throw std::runtime_error("factory failed"); }); }
    catch (const std::runtime_error&) { failed_factory = true; }
    assert(failed_factory && !controller->cache->has("failure"));
    store->put("broken", "secret-not-json");
    bool malformed = false;
    try { (void)controller->cache->get("broken"); } catch (const std::invalid_argument& error) { malformed = String{error.what()}.find("secret-not-json") == String::npos; }
    assert(malformed);

    const String binary{"a\0b", 3};
    const auto stored_file = Json::parse(send(app.router(), "/file", binary).body());
    assert(stored_file.get("exists")->dump() == "true" && stored_file.get("size")->string() == "3");
    assert(stored_file.get("files")->as_array().front().string() == "report.bin");
    assert(send(app.router(), "/file").body() == binary);
    assert(send(app.router(), "/archive", binary).body() == binary && !local->exists("monthly.csv"));
    const auto operations = Json::parse(send(app.router(), "/operations").body());
    assert(operations.get("copied")->dump() == "true" && operations.get("moved")->dump() == "true");
    assert(operations.get("removed")->dump() == "true" && operations.get("missing")->is_null());
    assert(operations.get("source")->dump() == "true" && operations.get("size")->string() == "3");
    auto selected = retainDisk(*controller->storage, "archive");
    std::weak_ptr<storage::Disk> old_disk = archive;
    manager->add("archive", std::make_shared<storage::LocalDisk>(files.root / "replacement"));
    archive.reset();
    assert(!old_disk.expired() && selected.get("monthly.csv") == binary);
    assert(!controller->storage->disk("archive").get("monthly.csv"));
    for (const auto path : {"../escape.bin", "/absolute.bin"}) {
        bool rejected = false;
        try { selected.put(path, binary); } catch (const std::exception&) { rejected = true; }
        assert(rejected);
    }
    assert(!std::filesystem::exists(files.root / "escape.bin"));
    bool unknown_disk = false;
    try { (void)controller->storage->disk("missing"); } catch (const std::logic_error&) { unknown_disk = true; }
    assert(unknown_disk);
    assert(send(app.router(), "/clear").status() == 204 && !controller->cache->has("summary"));

    std::optional<cache::Values> kept_cache;
    std::optional<storage::Service> kept_storage;
    std::optional<storage::FileStore> kept_disk;
    std::weak_ptr<cache::Store> adapter;
    {
        Application temporary;
        auto memory = std::make_shared<cache::MemoryStore>(); adapter = memory;
        auto disks = std::make_shared<storage::Manager>();
        disks->add("local", std::make_shared<storage::LocalDisk>(files.root / "retained"));
        temporary.provider<ServicesProvider>(ServiceOptions{.cache=memory, .storage=disks}); temporary.boot();
        kept_cache = retainCache(*temporary.container().resolve<cache::Values>());
        kept_storage = retainStorage(*temporary.container().resolve<storage::Service>());
        kept_disk = retainDisk(*kept_storage, "local");
    }
    assert(!adapter.expired());
    kept_cache->put("owned", Json{String{"alive"}});
    assert(kept_cache->get("owned")->string() == "alive");
    kept_storage->put("owned.bin", binary);
    kept_storage.reset();
    assert(kept_disk->get("owned.bin") == binary);
    kept_cache.reset(); assert(adapter.expired());
    bool missing_binding = false;
    Application unconfigured; unconfigured.provider<ServicesProvider>(); unconfigured.boot();
    try { (void)ServiceController::make(unconfigured.container()); } catch (const std::logic_error&) { missing_binding = true; }
    assert(missing_binding);
}
