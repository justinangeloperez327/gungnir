#include "structured_application_program.cpp"
#include <gungnir/language/compiler.hpp>
#include <gungnir/language/cpp_ir.hpp>
#include <gungnir/logging/memory_sink.hpp>
#include <gungnir/observability/memory_span_sink.hpp>
#include <gungnir/observability/memory_metric_sink.hpp>
#include <gungnir/session/memory_store.hpp>
#include <gungnir/storage/local_disk.hpp>
#include <gungnir/queue/memory_driver.hpp>
#ifdef GUNGNIR_WITH_SQLITE
#include <gungnir/database/sqlite.hpp>
#endif
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <locale>

using namespace gungnir;
#define CHECK(...) do { if (!(__VA_ARGS__)) { std::cerr << __FILE__ << ":" << __LINE__ << ": failed check: " << #__VA_ARGS__ << std::endl; std::exit(EXIT_FAILURE); } } while (false)
namespace {
struct Files {
    std::filesystem::path root = std::filesystem::temp_directory_path() / ("gungnir-application-" + security::random_token(8));
    Files() { std::filesystem::create_directories(root); }
    ~Files() { std::error_code error; std::filesystem::remove_all(root, error); }
    void write(const String& path, const String& text) const {
        const auto target = root / path; std::filesystem::create_directories(target.parent_path());
        std::ofstream output{target, std::ios::binary}; output << text; CHECK(output.good());
    }
};
struct DecimalComma : std::numpunct<char> { char do_decimal_point() const override { return ','; } };
struct BrokenLog : logging::Sink { void write(const logging::Record&) override { throw std::runtime_error("sink failed"); } };
struct Spans : observability::SpanSink {
    std::vector<observability::SpanRecord> records; int flushed{}, stopped{}; bool fail_flush{};
    void export_span(const observability::SpanRecord& record) override { records.push_back(record); }
    void flush() override { ++flushed; if (fail_flush) throw std::runtime_error("flush failed"); }
    void shutdown() override { ++stopped; }
};
struct Metrics : observability::MetricSink {
    std::vector<observability::MetricPoint> records; int flushed{}, stopped{};
    void export_metric(const observability::MetricPoint& record) override { records.push_back(record); }
    void flush() override { ++flushed; }
    void shutdown() override { ++stopped; }
};
struct Exports {
    std::shared_ptr<logging::MemorySink> logs = std::make_shared<logging::MemorySink>();
    std::shared_ptr<Spans> spans = std::make_shared<Spans>();
    std::shared_ptr<Metrics> metrics = std::make_shared<Metrics>();
    std::shared_ptr<logging::Logger> logger = std::make_shared<logging::Logger>();
    std::shared_ptr<observability::Tracer> tracer = std::make_shared<observability::Tracer>(spans);
    std::shared_ptr<observability::Meter> meter = std::make_shared<observability::Meter>(metrics);
    Exports() { logger->sink(std::make_shared<BrokenLog>()).sink(logs); }
    ServiceOptions options() const { ServiceOptions result; result.logger=logger; result.tracer=tracer; result.meter=meter; return result; }
};
void reject(const String& body, const String& prelude = {}) {
    const auto source = prelude + " function void invalid(Config config, Logger logger, Telemetry telemetry, Span span, Request request) {" + body + "}";
    const auto emitted = language::Compiler{}.compile(source,"application-invalid.gnr");
    language::CompilerOptions options; options.validate_only=true;
    const auto checked = language::Compiler{}.compile(source,"application-invalid.gnr",options);
    CHECK(!emitted.success() && !checked.success() && emitted.code.empty() && checked.code.empty());
    CHECK(emitted.diagnostics.size()==checked.diagnostics.size());
    for (std::size_t i=0;i<emitted.diagnostics.size();++i) {
        const auto& a=emitted.diagnostics[i]; const auto& b=checked.diagnostics[i];
        CHECK(a.code==b.code && a.message==b.message && a.span.begin_offset==b.span.begin_offset && a.span.end_offset==b.span.end_offset);
    }
}
template<class F> void fails(F action) { bool failed=false; try { action(); } catch (const std::exception&) { failed=true; } CHECK(failed); }
template<class T> void factory(Application& app) {
    app.bind<T>([](Container& c, ServiceScope* scope) { return scope ? T::make(*scope) : T::make(c); });
}
Response send(Application& app, String path, String actor = {}) {
    Request request{http::Method::get,std::move(path)};
    request.set_header("Accept","application/json"); request.set_header("Authorization","private-bearer");
    auto sessions=session::middleware(std::make_shared<session::MemoryStore>());
    auto identities=auth::session([](std::string_view id) {return std::optional{auth::Identity{String{id}}};});
    auto task=sessions(request,[&](Request& current)->Task<Response> {
        if (!actor.empty()) current.session().put("_gungnir_auth_user",actor);
        co_return co_await identities(current,[&](Request& authenticated)->Task<Response> {
            co_return co_await app.router().dispatch(authenticated);
        });
    });
    task.run_inline(); CHECK(task.done()); return task.operator co_await().await_resume();
}
struct Pause {
    std::coroutine_handle<>& continuation;
    bool await_ready() const { return false; }
    void await_suspend(std::coroutine_handle<> handle) { continuation=handle; }
    void await_resume() const {}
};
Task<Response> paused(std::coroutine_handle<>& continuation) { co_await Pause{continuation}; co_return Response::text("resumed"); }
Task<bool> start(Task<Response>& task, std::optional<Response>& result) { result=co_await task; co_return true; }
struct FailingRemember : auth::RememberStore {
    void put(String,String,std::chrono::system_clock::time_point) override { throw std::runtime_error("store unavailable"); }
    std::optional<String> consume(std::string_view) override { return {}; }
    void revoke(std::string_view) override {}
};
}
int main() {
    std::cerr << "application: canonical diagnostics\n";
    for (const auto& body : {"config.string(1);", "config.integer('x',true);", "Config::get('x');", "const value=Config;", "const value=Config();", "const value=Logger();", "const value=Telemetry();", "const value=Span();", "json(config);", "json({service:logger});", "logger.info('x',[]);", "logger.error('x',{data:[1]});", "telemetry.span('');", "telemetry.counter('x',-1);", "telemetry.gauge('x','bad');", "telemetry.histogram('x',1,{data:{}});", "span.attribute(1,'x');", "view('x',1);", "view('x',[]);", "authorize(request,'view',1);", "authorize(request,'view',config);"}) reject(body);
    reject("", "model M {} function void optional(Request request,M? resource) {authorize(request,'view',resource);} ");
    reject("", "model Invalid {Config config;} ");
    const auto valid=language::Compiler{}.compile("function void f(Logger log,Telemetry t) {log.info('x',{id:1});const s=t.span('work');s.attribute('k','v').error('failed');s.end();}","application-ir.gnr");
    CHECK(valid.success()); CHECK(language::CppIrVerifier{}.verify(language::CppIrLowerer{}.lower(*valid.validated,false)).success());

    Files files;
    files.write("views/layouts/app.html","<html><title>{{#yield 'title'}}default{{/yield}}</title><main>{{#yield 'content'}}empty{{/yield}}</main></html>");
    files.write("views/components/panel.html","<aside class=\"{{kind}}\">{{{slot}}}</aside>");
    files.write("views/partials/user.html","<span>{{ user.name }}:{{ user.password }}</span>");
    files.write("views/users/index.html", "{{#layout 'layouts/app'}}{{#section 'title'}}Users{{/section}}{{#section 'content'}}{{#if users}}yes{{else}}no{{/if}}{{#unless empty}}empty{{/unless}}{{#each users}}[{{loop.index}}:{{loop.first}}:{{loop.last}}:{{loop.count}}]{{> 'partials/user' user=this }}{{else}}none{{/each}}{{#component 'components/panel' kind='safe'}}{{ unsafe }}{{/component}}{{ unsafe }}|{{{trusted}}}|{{large}}|{{fraction}}|{{shout(unsafe)}}{{/section}}{{/layout}}");
    files.write("views/projects/show.html","<h1>{{ project.title }}</h1><p>{{app}}</p>");
    files.write("views/numbers.html","{{large}}|{{fraction}}|{{flag}}|{{zero}}");
    files.write("views/recursive.html","{{> 'recursive'}}");
    files.write("outside.html","private-outside");
    Exports exports;
    Application app; app.view_root(files.root/"views"); app.provider<ServicesProvider>(exports.options());
    app.config().set("app.name","Application A").set("app.limit",Int64{17}).set("app.enabled",true).set("app.ratio",3.5).set("stored.null",config::Value{std::monostate{}});
    app.views().helper("shout",[](const std::vector<view::Value>& values) { return view::Value{values.empty() ? "" : values.front().string()+"!"}; });
    factory<ApplicationController>(app); factory<ApplicationMiddleware>(app);
    app.boot(); auto active=app.activate();
    gnr_register_routes(app);

    std::cerr << "application: generated views, visibility and numbers\n";
    ApplicationActor first,second; first.id=1;first.name="<Freya>";first.password="hidden-secret";second.id=2;second.name="Odin";second.password="second-secret";
    const auto previous=std::locale::global(std::locale(std::locale::classic(),new DecimalComma));
    const auto rendered=renderApplicationModels({first,second},"<script>x</script>",std::numeric_limits<UInt64>::max(),3.5);
    std::locale::global(previous);
    CHECK(rendered.status()==201); const String body{rendered.body()};
    CHECK(body.find("<title>Users</title>")!=body.npos && body.find("yesempty")!=body.npos);
    CHECK(body.find("[0:true:false:2]")!=body.npos && body.find("[1:false:true:2]")!=body.npos);
    CHECK(body.find("&lt;Freya&gt;")!=body.npos && body.find("hidden-secret")==body.npos && body.find("second-secret")==body.npos);
    CHECK(body.find("<aside class=\"safe\">&lt;script&gt;x&lt;/script&gt;</aside>")!=body.npos);
    CHECK(body.find("<b>trusted</b>")!=body.npos && body.find("18446744073709551615|3.5")!=body.npos);
    CHECK(body.find("&lt;script&gt;x&lt;/script&gt;!")!=body.npos && body.find("<script>")==body.npos);
    CHECK(renderApplicationModels({},"",0,0).body().find("noempty")!=std::string_view::npos);
    CHECK(renderApplicationView(Json::parse(R"({"large":18446744073709551615,"fraction":3.5,"flag":false,"zero":0})")).body()=="18446744073709551615|3.5|false|0");
    auto controller=ApplicationController::make(app.container());
    for (const auto name : {"../outside","/outside","recursive"}) fails([&] { controller->namedView(name); });
    fails([&] { controller->dynamicView(Json::array({})); });
    files.write("views/huge.html",String(9*1024*1024,'x')); fails([&] { controller->namedView("huge"); });
    const auto configuration=Json::parse(send(app,"/configuration").body());
    CHECK(configuration.get("name")->string()=="Application A" && configuration.get("limit")->string()=="17");
    CHECK(configuration.get("present")->string()=="true"); CHECK(configuration.get("missing")->is_null());
    auto config_service=app.container().resolve<config::Service>(); CHECK(!config_service->get("absent") && config_service->get("stored.null") && config_service->get("stored.null")->is_null());
    app.config().set("invalid.integer",std::numeric_limits<double>::infinity()).set("invalid.range",1e30).set("invalid.number","nan");
    fails([&] {(void)config_service->integer("invalid.integer");});
    fails([&] {(void)config_service->integer("invalid.range");});
    fails([&] {(void)config_service->number("invalid.number");});

    std::cerr << "application: actor resolution and bound-resource authorization\n";
    auto authorization=app.container().resolve<auth::ResourceAuthorization>();
    register_policy(app,ApplicationPolicy::make(app.container()));
#ifdef GUNGNIR_WITH_SQLITE
    database::Settings settings; settings.backend=database::Backend::sqlite; settings.database=(files.root/"application.sqlite").string();
    app.database("default",database::Backend::sqlite,[settings] {return std::make_shared<database::SQLiteDriver>(settings);});
    auto db=app.database().connection();
    db->execute("CREATE TABLE application_users (id INTEGER PRIMARY KEY,name TEXT,password TEXT)");
    db->execute("CREATE TABLE application_projects (id INTEGER PRIMARY KEY,owner_id INTEGER,title TEXT)");
    db->execute("INSERT INTO application_users VALUES (1,'Freya','private-hash'),(2,'Odin','other-hash')");
    db->execute("INSERT INTO application_projects VALUES (1,1,'<Project>')");
    db.reset();
    const auto anonymous=send(app,"/projects/1");
    if (anonymous.status()!=401) std::cerr << "anonymous response: " << anonymous.status() << " " << anonymous.body() << '\n';
    CHECK(anonymous.status()==401);
    CHECK(send(app,"/projects/1","2").status()==403);
    const auto allowed=send(app,"/projects/1","1"); CHECK(allowed.status()==200);
    CHECK(allowed.body().find("&lt;Project&gt;")!=std::string_view::npos);
    CHECK(send(app,"/projects/999","1").status()==404);
    CHECK(send(app,"/projects/1","missing").status()==403);
    authorization->actor<ApplicationActor>([first](const auth::Identity&) {return std::optional{first};});
    register_policy(app,ApplicationPolicy::make(app.container()));
    CHECK(send(app,"/projects/1","custom-identity").status()==200);
    CHECK(app.database().pool_stats().leased==0);
#else
    authorization->actor<ApplicationActor>([first](const auth::Identity&) {return std::optional{first};});
    ApplicationProject project; project.id=1; project.owner_id=1; project.title="<Project>";
    app.router().get("/projects",[controller,project](Request& request) { return controller->show(request,project); });
    CHECK(send(app,"/projects","custom-identity").status()==200);
#endif
    CHECK(!exports.logs->records().empty());
    for (const auto& record:exports.logs->records()) if(record.message=="Project viewed") CHECK(record.context.at("authorization")=="[redacted]");
    CHECK(!exports.spans->records.empty()); for(const auto& span:exports.spans->records) if(span.name=="project.view") CHECK(span.attributes.at("password")=="[redacted]");
    CHECK(exports.metrics->records.size()>=3);

    std::cerr << "application: request and worker dependency scopes\n";
    Application scoped; Exports scoped_exports; auto scoped_options=scoped_exports.options();
    auto queue=std::make_shared<queue::MemoryDriver>(); auto disks=std::make_shared<storage::Manager>();
    disks->add("local",std::make_shared<storage::LocalDisk>(files.root/"jobs")); scoped_options.queue=queue; scoped_options.storage=disks;
    scoped.provider<ServicesProvider>(scoped_options); int scope_ids{};
    scoped.scoped<config::Service>([&](ServiceScope&) {
        auto values=std::make_shared<config::Repository>(); values->set("scope.id",Int64{++scope_ids}).set("app.name","scoped"); return config::Service{values};
    });
    factory<ApplicationController>(scoped); factory<ApplicationMiddleware>(scoped); scoped.middleware<ApplicationMiddleware>(); scoped.boot();
    gnr_register_routes(scoped);
    const auto a=send(scoped,"/scope"),b=send(scoped,"/scope");
    CHECK(a.body()==a.header("X-Scope") && b.body()==b.header("X-Scope") && a.body()!=b.body());
    fails([&] { (void)scoped.container().resolve<config::Service>(); });
    register_job<ApplicationJob>(scoped);
    auto dispatcher=scoped.container().resolve<queue::Service>(); dispatcher->dispatch(ApplicationJob{"a"}); dispatcher->dispatch(ApplicationJob{"b"});
    auto worker=scoped.container().resolve<queue::Worker>(); CHECK(worker->run_one() && worker->run_one());
    auto storage=scoped.container().resolve<storage::Service>(); CHECK(storage->get("a")!=storage->get("b"));

    std::cerr << "application: actual suspension and retained owners\n";
    std::coroutine_handle<> resume; Request request{http::Method::get,"/suspended"};
    std::weak_ptr<config::Repository> retained_repository; std::optional<Response> resumed;
    Task<Response> pending; Task<bool> started;
    { Application temporary; Exports temporary_exports; temporary.provider<ServicesProvider>(temporary_exports.options());
      temporary.config().set("app.name","retained"); temporary.boot(); auto scope=temporary.activate();
      retained_repository=temporary.container().resolve<config::Repository>();
      temporary.container().instance<ApplicationMiddleware>(ApplicationMiddleware::make(temporary.container()));
      auto handler=http::make_middleware<ApplicationMiddleware>(temporary.container());
      pending=handler(request,[&](Request&){return paused(resume);});
      started=start(pending,resumed); started.run_inline(); CHECK(resume && !resumed);
    }
    CHECK(!retained_repository.expired());
    resume.resume(); CHECK(resumed && resumed->header("X-Application")=="retained");
    pending={}; started={}; CHECK(retained_repository.expired());

    std::cerr << "application: sink isolation and cleanup\n";
    Exports other_exports; Application other; other.provider<ServicesProvider>(other_exports.options()); other.boot();
    { auto owner=other.activate(); auto span=observability::global_tracer()->start_span("other"); span.end(); observability::global_meter()->counter("other.counter").add(); }
    CHECK(other_exports.spans->records.size()==1 && other_exports.metrics->records.size()==1);
    CHECK(std::none_of(exports.spans->records.begin(),exports.spans->records.end(),[](const auto& span){return span.name=="other";}));
    const auto data=Json::object({{"PASSWORD",Json{"private"}},{"value",Json{Int64{7}}}});
    controller->logger->info("safe",data); CHECK(exports.logs->records().back().context.at("PASSWORD")=="[redacted]");
    fails([&] {controller->logger->info("bad",Json::array({}));});
    fails([&] {controller->telemetry->counter("bad",-1);});
    fails([&] {controller->telemetry->gauge("bad",std::numeric_limits<double>::infinity());});
    exports.spans->fail_flush=true; app.shutdown(); CHECK(exports.spans->flushed==1 && exports.spans->stopped==1);
    CHECK(exports.metrics->flushed==1 && exports.metrics->stopped==1 && app.shutdown_errors().size()==1);
    other.shutdown(); scoped.shutdown();
#ifdef GUNGNIR_WITH_PASSWORD
    std::cerr << "application: persistent-store failure is fail closed\n";
    auth::SessionGuard guard{[](std::string_view){return std::optional<auth::PasswordIdentity>{};},[](std::string_view){return std::optional<auth::Identity>{};},std::make_shared<FailingRemember>()};
    Application failed_login;
    failed_login.router().get("/failed",[&](Request& request) {
        const auto guest=request.session().id(); Response response;
        fails([&] {guard.login(request,response,auth::Identity{"1"},true);});
        CHECK(!request.authenticated() && request.session().id()==guest && response.cookies().empty());
        return response;
    });
    CHECK(send(failed_login,"/failed").status()==200);
#endif
    std::cout << "Generated views, authorization, configuration, DI and observability passed\n";
}
