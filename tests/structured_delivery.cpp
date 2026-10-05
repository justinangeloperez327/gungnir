#include "structured_delivery_program.cpp"
#include <gungnir/language/compiler.hpp>
#include <gungnir/mail/memory_transport.hpp>
#include <gungnir/queue/memory_driver.hpp>
#ifdef GUNGNIR_WITH_SQLITE
#include <gungnir/database/sqlite.hpp>
#endif
#include <coroutine>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace gungnir;
#define CHECK(...) do { if (!(__VA_ARGS__)) { std::cerr << __FILE__ << ':' << __LINE__ << ": " << #__VA_ARGS__ << std::endl; std::exit(EXIT_FAILURE); } } while(false)
namespace {
template<class F> void fails(F action) { bool caught{}; try { action(); } catch (const std::exception&) { caught = true; } CHECK(caught); }
struct Channel : notifications::Channel {
    std::vector<String> recipients, names; std::vector<Json> payloads; bool broken{};
    void send(std::string_view recipient, const notifications::Notification& notice) override {
        if (broken) throw std::runtime_error("channel failed");
        recipients.emplace_back(recipient); names.emplace_back(notice.name());
        if (auto data = notice.database_payload()) payloads.push_back(*data);
    }
};
struct RetryTransport : mail::Transport {
    int calls{}; bool broken = true; mail::MemoryTransport accepted;
    void send(const mail::Message& value) override { ++calls; if (broken) throw std::runtime_error("transport failed"); accepted.send(value); }
};
ServiceOptions services(const std::shared_ptr<mail::Transport>& transport, const std::shared_ptr<queue::Driver>& queue = {}) {
    ServiceOptions result; result.mail = transport; result.sender = {"sender@example.test", "Gungnir"}; result.queue = queue; return result;
}
void reject(const String& body) {
    const String declarations = "model User { fillable = ['email']; } model Other {} mail Welcome { subject() { return 'Welcome'; } } notification Notice { via(User user) { return ['mail']; } toMail(User user) { return Welcome(); } } ";
    const auto emitted = language::Compiler{}.compile(declarations + body, "invalid-delivery.gnr");
    language::CompilerOptions options; options.validate_only = true;
    const auto checked = language::Compiler{}.compile(declarations + body, "invalid-delivery.gnr", options);
    CHECK(!emitted.success() && !checked.success() && emitted.code.empty() && checked.code.empty());
    CHECK(emitted.diagnostics.size() == checked.diagnostics.size());
    for (std::size_t i = 0; i < emitted.diagnostics.size(); ++i) {
        const auto& a = emitted.diagnostics[i]; const auto& b = checked.diagnostics[i];
        CHECK(a.code == b.code && a.message == b.message && a.span.valid && b.span.valid);
        CHECK(a.span.begin_offset == b.span.begin_offset && a.span.end_offset == b.span.end_offset);
    }
}
struct Pause {
    std::coroutine_handle<>* handle;
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> value) const noexcept { *handle = value; }
    void await_resume() const noexcept {}
};
Task<Response> pause(Request, std::coroutine_handle<>& handle) { co_await Pause{&handle}; co_return Response::text("resumed"); }
Task<void> complete(Task<Response> task) { const auto result = co_await task; CHECK(result.body() == "resumed"); }
Task<int> start(Task<void>& task) { co_await task; co_return 0; }
}
int main() {
    std::cerr << "delivery: canonical diagnostics\n";
    for (const auto& source : {
        "function void invalid(Mail mail) { mail.to(1); }",
        "function void invalid(Mail mail) { mail.to('a@example.test').send(1); }",
        "function void invalid(Mail mail, Welcome? value) { mail.to('a@example.test').send(value); }",
        "function void invalid(Mail mail) { mail.to('a@example.test').queue(Welcome(),0); }",
        "function void invalid(Mail mail) { mail.to('a@example.test').queue(Welcome(),4294967296); }",
        "function void invalid(Mail mail) { mail.to('a@example.test').attach('file'); }",
        "function void invalid(Mail mail) { mail.to('a@example.test').cc('a@example.test',2); }",
        "function void invalid(Mail mail) { mail.to('a@example.test').send(Welcome(),1); }",
        "function void invalid(Notifications notices, Other user) { notices.send(user,Notice()); }",
        "function void invalid(Notifications notices, User? user) { notices.send(user,Notice()); }",
        "function void invalid(Notifications notices, User user, Notice? value) { notices.queue(user,value); }",
        "function void invalid(Notifications notices, User user) { notices.queue(user,Notice(),-1); }",
        "function void invalid() { Mail::to('a@example.test'); }",
        "function void invalid(Mail mail) { mail::to('a@example.test'); }",
        "function Json invalid(Mail mail) { return {service: mail.to('a@example.test')}; }",
        "job Invalid { PendingMail pending; handle() {} }",
        "model Invalid { Mail mail; }",
        "controller Invalid { inject PendingMail pending; index() { return noContent(); } }"
    }) reject(source);

    auto transport = std::make_shared<mail::MemoryTransport>();
    auto driver = std::make_shared<queue::MemoryDriver>();
    Application app; app.provider<ServicesProvider>(services(transport, driver)); app.boot();
    auto mail = app.container().resolve<mail::Service>();
    auto notices = app.container().resolve<notifications::Service>();
    auto worker = app.container().resolve<queue::Worker>();
    auto native = app.container().resolve<notifications::Manager>();
    auto database = std::make_shared<Channel>(); auto audit = std::make_shared<Channel>();
    native->channel("database", database).channel("audit", audit);
    notices->route<DeliveryRecipient>("audit", [](const auto& recipient) { return "audit-" + std::to_string(recipient.id.get()); });

    std::cerr << "delivery: generated controllers and recipient routing\n";
    auto controller = DeliveryController::make(app.container());
    CHECK(controller->send().status() == 204);
    auto messages = transport->messages(); CHECK(messages.size() == 2);
    CHECK(messages[0].sender().email == "sender@example.test" && messages[0].recipients()[0].name == "Ada");
    CHECK(messages[0].cc_recipients().size() == 1 && messages[0].bcc_recipients().size() == 1);
    CHECK(messages[0].reply_address()->email == "reply@example.test" && messages[0].attachments()[0].contents == String("a\0b",3));
    CHECK(messages[1].recipients().size() == 1 && messages[1].recipients()[0].email == "ada@example.test");
    CHECK(database->recipients == std::vector<String>{"42"} && audit->recipients == std::vector<String>{"audit-42"});
    CHECK(!database->payloads[0].get("user")->get("password") && !database->payloads[0].get("user")->get("email"));
    auto recipient = deliveryRecipient();
    notices->send(recipient, DeliveryMailOnly{}); CHECK(transport->messages().size() == 3);
    fails([&] { notices->send(recipient, DeliveryMissing{}); }); CHECK(transport->messages().size() == 3);
    fails([&] { notices->send(recipient, DeliveryDuplicate{}); }); CHECK(transport->messages().size() == 3);
    fails([&] { notices->queue(recipient, DeliveryNoMail{}); }); CHECK(!worker->run_one());
    auto absent_email = recipient; absent_email.email = "";
    fails([&] { notices->send(absent_email, DeliveryMailOnly{}); });
    auto account = DeliveryAccount(); account.key = "customer-A"; account.email = "a@example.test";
    notices->send(account, DeliveryAccountNotice{}); CHECK(database->recipients.back() == "customer-A");
    const auto original = mail->to("one@example.test");
    original.cc("ignored@example.test").send(DeliveryWelcome{"Copy"});
    original.send(DeliveryWelcome{"Original"}); CHECK(transport->messages().back().cc_recipients().empty());
    fails([&] { mail->to("bad\r\n@example.test"); });
    fails([&] { mail->to("a@example.test").send(DeliveryWelcome{"bad\nSubject"}); });
    fails([&] { original.attach(String("bad\0name",8), "bytes"); });
    fails([&] { original.replyTo("invalid"); });
    const auto override_sender = original.from("override@example.test", "Override");
    override_sender.send(DeliveryWelcome{"Sender"}); CHECK(transport->messages().back().sender().email == "override@example.test");

    std::cerr << "delivery: queued snapshots, payload validation and native retries\n";
    const auto before = transport->messages().size();
    CHECK(!deliveryQueue(*mail).empty()); CHECK(transport->messages().size() == before);
    CHECK(worker->run_one()); CHECK(transport->messages().size() == before + 1); CHECK(!worker->run_one());
    CHECK(transport->messages().back().attachments()[0].contents == String("a\0b",3));
    CHECK(!deliveryQueueNotice(*notices, recipient).empty()); recipient.name = "Changed"; recipient.email = "changed@example.test";
    CHECK(worker->run_one()); CHECK(transport->messages().back().subject_line() == "Welcome Ada");
    CHECK(transport->messages().back().recipients()[0].email == "ada@example.test");
    CHECK(database->payloads.back().get("title")->string() == "Queued notice");
    fails([&] { deliveryQueue(*mail, 0); }); fails([&] { deliveryQueue(*mail, -1); }); CHECK(!worker->run_one());
    auto raw = transport->messages()[0]; raw.attach({"binary.bin", String("\xff\0\x80",3)});
    const auto decoded = mail::delivery::Job::read(mail::delivery::Job{raw}.payload());
    CHECK(decoded.attachments().back().contents == String("\xff\0\x80",3));
    fails([&] { mail::delivery::Job::read("{\"version\":2,\"message\":{}}"); });
    fails([&] { notifications::Snapshot::read("{\"version\":1,\"name\":42}"); });
    const auto sent = transport->messages().size();
    driver->push({.id="malformed",.name=String{mail::delivery::Job::job_name},.payload="{}"});
    CHECK(worker->run_one()); CHECK(driver->failed_jobs().size() == 1 && transport->messages().size() == sent);
    CHECK(driver->forget_failed("malformed"));
    auto retry_transport = std::make_shared<RetryTransport>(); auto retry_driver = std::make_shared<queue::MemoryDriver>();
    Application retry; retry.provider<ServicesProvider>(services(retry_transport,retry_driver)); retry.boot();
    auto retry_mail = retry.container().resolve<mail::Service>(); auto retry_worker = retry.container().resolve<queue::Worker>();
    CHECK(!deliveryQueue(*retry_mail,2).empty()); CHECK(retry_worker->run_one()); CHECK(retry_driver->failed_jobs().empty());
    CHECK(retry_worker->run_one()); CHECK(retry_driver->failed_jobs().size() == 1 && retry_transport->calls == 2);
    retry_transport->broken = false; CHECK(retry_driver->retry_failed(retry_driver->failed_jobs()[0].id));
    CHECK(retry_worker->run_one()); CHECK(retry_transport->accepted.messages().size() == 1 && retry_driver->failed_jobs().empty());
    auto no_queue_transport = std::make_shared<mail::MemoryTransport>();
    mail::Service no_queue{no_queue_transport,{"sender@example.test",{}}};
    fails([&] { deliveryQueue(no_queue); });

#ifdef GUNGNIR_WITH_SQLITE
    std::cerr << "delivery: SQLite notification persistence and commit ordering\n";
    const auto path = std::filesystem::temp_directory_path() / ("gungnir-delivery-" + security::random_token(8) + ".sqlite");
    database::Settings settings; settings.backend = database::Backend::sqlite; settings.database = path.string();
    app.database("default",database::Backend::sqlite,[settings] { return std::make_shared<database::SQLiteDriver>(settings); });
    auto db = app.database().connection();
    db->execute("CREATE TABLE notifications (id TEXT PRIMARY KEY,type TEXT,recipient TEXT,data TEXT)");
    native->channel("database",std::make_shared<notifications::DatabaseChannel>());
    { detail::ExecutionScope active{app.execution_context()};
      database::runtime::detail::ConnectionScope connection_scope{db};
      notices->send(account,DeliveryAccountNotice{});
      const auto rows = db->execute("SELECT recipient,data FROM notifications"); CHECK(rows.rows.size() == 1);
      db->begin(); CHECK(!deliveryQueue(*mail).empty()); CHECK(!worker->run_one()); db->rollback(); CHECK(!worker->run_one());
      db->begin(); CHECK(!deliveryQueue(*mail).empty()); CHECK(!worker->run_one()); db->commit(); CHECK(worker->run_one());
      db->begin(); CHECK(!deliveryQueue(*mail,1,false).empty()); CHECK(worker->run_one()); db->rollback();
      CHECK(!deliveryQueueNotice(*notices,deliveryRecipient()).empty());
    }
    db.reset(); CHECK(worker->run_one());
    CHECK(app.database().connection()->execute("SELECT recipient FROM notifications").rows.size() == 2);
    std::error_code database_error; std::filesystem::remove(path,database_error);
#endif

    std::cerr << "delivery: view composition and coroutine owner retention\n";
    const auto views = std::filesystem::temp_directory_path() / ("gungnir-delivery-views-" + security::random_token(8));
    std::filesystem::create_directories(views / "mail");
    { std::ofstream file{views / "mail/welcome.html"}; file << "<p>{{ name }}</p>"; }
    app.views().root(views);
    { detail::ExecutionScope active{app.execution_context()}; original.send(DeliveryView{"<Ada>"}); }
    CHECK(transport->messages().back().html_body() == "<p>&lt;Ada&gt;</p>");
    std::error_code error; std::filesystem::remove_all(views,error);
    std::weak_ptr<detail::ExecutionContext> application_owner;
    auto retained = [&] { Application temporary; application_owner = temporary.execution_context(); temporary.provider<ServicesProvider>(services(transport)); temporary.boot(); return temporary.container().resolve<mail::Service>()->to("retained@example.test"); }();
    CHECK(application_owner.expired()); retained.send(DeliveryWelcome{"Retained"});
    CHECK(transport->messages().back().recipients()[0].email == "retained@example.test");
    std::coroutine_handle<> suspended;
    auto middleware = [&] { Application temporary; temporary.provider<ServicesProvider>(services(transport)); temporary.boot(); return DeliveryMiddleware::make(temporary.container()); }();
    Request retained_request{http::Method::get,"/"};
    Next next = [&](Request request) { return pause(std::move(request),suspended); };
    auto task = complete(middleware->handle(retained_request,next));
    auto starter = start(task); starter.run_inline(); CHECK(suspended && !task.done()); middleware.reset();
    suspended.resume(); CHECK(task.done()); CHECK(transport->messages().back().recipients()[0].email == "middleware@example.test");
    std::cout << "Generated mail and notification delivery contract passed\n";
}
