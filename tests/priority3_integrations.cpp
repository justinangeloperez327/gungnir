#include <gungnir/gungnir.hpp>
#include <gungnir/core/services.hpp>
#include <gungnir/orm/orm.hpp>
#include <gungnir/cache/memory_store.hpp>
#include <gungnir/queue/memory_driver.hpp>
#include <gungnir/mail/memory_transport.hpp>
#include <gungnir/validation/validator.hpp>
#ifdef GUNGNIR_WITH_PASSWORD
#include <gungnir/auth/login.hpp>
#endif
#ifdef GUNGNIR_WITH_SQLITE
#include <gungnir/database/sqlite.hpp>
#endif
#include <cassert>
#include <fstream>
#include <array>
#include <limits>

using namespace gungnir;
class Record : public Model<Record> {
public:
    inline static constexpr Table table{"records"};
    inline static constexpr auto hidden=std::array<std::string_view,1>{"secret"};
    inline static constexpr auto visible=std::array<std::string_view,3>{"id","name","secret"};
    PrimaryKey<Int64> id;
    Field<String> name,secret;
    Field<Int64> owner_id;
};
template<> struct gungnir::model::Generated<Record> {
    inline static constexpr auto attributes=std::tuple{model::attribute("id",&Record::id),model::attribute("name",&Record::name),model::attribute("secret",&Record::secret),model::attribute("owner_id",&Record::owner_id)};
    inline static constexpr auto relations=std::tuple{};
};
class Owner : public Model<Owner> {
public:
    inline static constexpr Table table{"owners"};
    PrimaryKey<Int64> id;
    HasMany<Record> records{"owner_id"};
};
template<> struct gungnir::model::Generated<Owner> {
    inline static constexpr auto attributes=std::tuple{model::attribute("id",&Owner::id)};
    inline static constexpr auto relations=std::tuple{model::relation("records",&Owner::records)};
};
struct Job : queue::Job {
    std::string_view name() const noexcept override { return "test"; }
    std::string payload() const override { return "{}"; }
};
struct Notice : notifications::Notification {
    std::string_view name() const noexcept override { return "notice"; }
    std::vector<std::string> channels() const override { return {"mail"}; }
    std::optional<mail::Message> mail_message() const override { return mail::Message{}.subject("notice").text("hello").attach({"test.txt","payload"}); }
};
Response with_session(Request& request, http::Next action) {
    static auto store=std::make_shared<session::MemoryStore>();
    auto start=session::middleware(store);
    auto authenticate=auth::session([](std::string_view id) -> std::optional<auth::Identity> { return auth::Identity{.id=std::string{id}}; });
    auto task=start(request,[&](Request& current) -> Task<Response> { co_return co_await authenticate(current,action); });
    task.run_inline(); assert(task.done());
    return task.operator co_await().await_resume();
}
std::string cookie(const Response& response, std::string_view name) {
    for (const auto& item : response.cookies()) if (item.name==name) return item.value;
    return {};
}
int main() {
    auto record=Record::hydrate({{"id",Int64{7}},{"name",String{"Ada"}},{"secret",String{"password"}}});
    assert(record.attributes().contains("secret"));
    assert(!http::make_json(record).get("secret"));
    assert(!view::make_value(record).get("secret"));
    bool overflow=false; try { (void)model::value_cast<int>(model::AttributeValue{std::numeric_limits<UInt64>::max()}); } catch (const std::out_of_range&) { overflow=true; } assert(overflow);
    model::Decimal decimal{"12345678901234567890.12345678901234567890"};
    assert(http::make_json(model::AttributeValue{decimal}).string()==decimal.string());
    assert(model::value_cast<model::Decimal>(model::AttributeValue{decimal})==decimal);
    orm::Collection<int> numbers{std::vector<int>{3,1,1,2}};
    auto identity=[](int value) { return value; };
    assert(numbers.sort_by(identity).first()==1 && numbers.unique(identity).count()==3);
    assert(numbers.group_by(identity).at(1).size()==2 && numbers.sum(identity)==7);
    assert(numbers.chunk(3).back().size()==1 && numbers.take(2).size()==2 && numbers.skip(100).empty());
    assert(numbers.reduce(0,[](int a,int b) { return a+b; })==7);
    auto input=Json::parse(R"({"users":[{"email":"a@example.com","admin":true},{"email":"bad"}],"enabled":false,"count":4,"nothing":null})");
    auto checked=validation::Validator::check(input,{{"users.*.email","required|email"},{"enabled","boolean"},{"count","integer"},{"nothing","nullable|string"}});
    assert(checked.errors.contains("users.1.email"));
    assert(checked.values.get("enabled")->is_boolean() && checked.values.get("count")->is_integer());
    assert(!checked.values.get("users")->as_array()[0].get("admin"));
    assert(checked.values.get("nothing")->is_null());
    bool unknown=false; try { validation::Validator::check(Json::object({}),{{"missing","sometimes|mystery"}}); } catch (const std::logic_error&) { unknown=true; } assert(unknown);
    Request request{http::Method::post,"/?query=1",R"({"enabled":false,"nested":{"items":[1,2]}})"}; request.set_header("Content-Type","application/json");
    assert(request.structured_input().get("nested")->is_object());
    assert(request.validate_structured({{"enabled","boolean"}}).get("enabled")->is_boolean());
    {
        auto root=std::filesystem::temp_directory_path()/security::random_token(8);
        std::filesystem::create_directories(root);
        std::ofstream(root/"layout.html") << "<title>{{#yield 'title'}}Default{{/yield}}</title>{{#yield 'content'}}Empty{{/yield}}";
        std::ofstream(root/"part.html") << "<b>{{ title }}</b>";
        std::ofstream(root/"component.html") << "<aside>{{ kind }}:{{{ slot }}}</aside>";
        view::Engine engine{root};
        engine.helper("upper",[](const std::vector<view::Value>& values) { return view::Value{values.at(0).string()+"!"}; });
        view::Data data{{"title","<Ada>"},{"active",true},{"users",std::vector<int>{10,20}},{"injected","{{ title }}"}};
        assert(engine.render_text("{{#if active}}yes{{else}}no{{/if}}",data)=="yes");
        assert(engine.render_text("{{#each users}}{{loop.index}}:{{this}};{{/each}}",data)=="0:10;1:20;");
        assert(engine.render_text("{{> 'part' }}",data)=="<b>&lt;Ada&gt;</b>");
        assert(engine.render_text("{{#layout 'layout'}}{{#section 'title'}}Hi{{/section}}{{#section 'content'}}{{> 'part'}}{{/section}}{{/layout}}",data)=="<title>Hi</title><b>&lt;Ada&gt;</b>");
        assert(engine.render_text("{{#component 'component' kind='ok'}}<b>{{title}}</b>{{/component}}",data)=="<aside>ok:<b>&lt;Ada&gt;</b></aside>");
        assert(engine.render_text("{{ upper(title) }}",data)=="&lt;Ada&gt;!");
        assert(engine.render_text("{{{ injected }}}",data)=="{{ title }}");
        bool malformed=false; try { engine.render_text("{{#if active}}x{{/each}}",data); } catch (const view::SyntaxError&) { malformed=true; } assert(malformed);
        std::filesystem::remove_all(root);
    }
    auto memory_queue=std::make_shared<queue::MemoryDriver>();
    auto memory_mail=std::make_shared<mail::MemoryTransport>();
    Application app; app.provider<ServicesProvider>(ServiceOptions{.cache=std::make_shared<cache::MemoryStore>(),.queue=memory_queue,.mail=memory_mail,.sender={"sender@example.com","Gungnir"}}); app.boot();
    auto active=app.activate();
    auto cache=app.container().resolve<cache::Repository>(); cache->put("key","value"); assert(cache->get("key")=="value");
    app.container().resolve<notifications::Manager>()->send("user@example.com",Notice{});
    assert(memory_mail->messages().size()==1 && memory_mail->messages()[0].attachments().size()==1);
    auto authorization=app.container().resolve<auth::ResourceAuthorization>();
    authorization->define<Record,Record>("update",[](const Record& actor,const Record& resource) { return actor.id.get()==resource.id.get(); });
    authorization->actor<Record>([record](const auth::Identity& id) -> std::optional<Record> { return id.id=="7" ? std::optional{record} : std::nullopt; });
    Request secured{http::Method::get,"/"};
    with_session(secured,[](Request& current) -> Task<Response> { current.auth().login(auth::Identity{.id="7"}); co_return Response{}; });
    assert(authorization->inspect("update",secured,record).allowed);
    auto other=record; other.id=8; assert(!authorization->inspect("update",secured,other).allowed);
#ifdef GUNGNIR_WITH_SQLITE
    app.database("default",database::Backend::sqlite,[] { return std::make_shared<database::SQLiteDriver>(database::Settings{.backend=database::Backend::sqlite,.database=":memory:"}); });
    auto connection=app.database().connection();
    connection->execute("CREATE TABLE records (id INTEGER PRIMARY KEY, name TEXT, secret TEXT, owner_id INTEGER)");
    auto inserted=connection->execute("INSERT INTO records(name, secret) VALUES (?, ?)",{String{"Ada"},String{"password"}});
    assert(inserted.affected_rows==1 && inserted.inserted_id);
    connection->execute("CREATE TABLE exact_values (value TEXT)");
    connection->execute("INSERT INTO exact_values VALUES (?)",{decimal});
    assert(model::value_cast<model::Decimal>(connection->execute("SELECT value FROM exact_values").rows[0].at("value"))==decimal);
    auto connection_scope=database::runtime::activate(connection);
    assert(!validation::Validator::check(validation::Input{{"name","Ada"}},{{"name","unique:records,name"}}).valid());
    assert(validation::Validator::check(validation::Input{{"name","Ada"}},{{"name","exists:records,name"}}).valid());
    connection->execute("CREATE TABLE owners (id INTEGER PRIMARY KEY)");
    connection->execute("INSERT INTO owners VALUES (1),(2)");
    connection->execute("INSERT INTO records (name, secret, owner_id) VALUES ('included','',1),('excluded','',1),('included','',2)");
    orm::RelationConstraint constraint{.predicates={{.column="name",.values={String{"included"}}}}};
    auto owners=Owner::query().with("records",constraint).order_by("id").get();
    assert(owners.size()==2 && owners[0].records.get().size()==1 && owners[1].records.get().size()==1);
    assert(owners[0].records.get().front().owner_id.get()==1 && owners[1].records.get().front().owner_id.get()==2);
    auto dispatcher=app.container().resolve<queue::Dispatcher>();
    { database::Transaction outer{connection}; outer.run([&] {
        dispatcher->dispatch(Job{}); assert(!memory_queue->pop());
        database::Transaction nested{connection}; nested.run([&] { dispatcher->dispatch(Job{}); });
        assert(!memory_queue->pop());
    }); }
    assert(memory_queue->pop() && memory_queue->pop() && !memory_queue->pop());
    { database::Transaction outer{connection}; outer.run([&] {
        database::Transaction nested{connection}; dispatcher->dispatch(Job{}); nested.rollback();
    }); }
    assert(!memory_queue->pop());
    { database::Transaction transaction{connection}; dispatcher->dispatch(Job{}); transaction.rollback(); }
    assert(!memory_queue->pop());
#endif
#ifdef GUNGNIR_WITH_PASSWORD
    const auto password=auth::Password::hash("secret");
    assert(auth::Password::verify("secret",password) && !auth::Password::verify("wrong",password));
    assert(!auth::Password::verify("secret","invalid") && !auth::Password::needs_rehash(password));
    auto remember=std::make_shared<auth::MemoryRememberStore>();
    auth::SessionGuard guard{[&](std::string_view name) -> std::optional<auth::PasswordIdentity> {
        if (name!="Ada") return std::nullopt; return auth::PasswordIdentity{auth::Identity{.id="7"},password};
    },[](std::string_view id) -> std::optional<auth::Identity> { return auth::Identity{.id=std::string{id}}; },remember};
    Request login{http::Method::post,"/login"};
    auto response=with_session(login,[&](Request& current) -> Task<Response> {
        const auto old=std::string{current.session().id()};
        Response result; assert(guard.attempt(current,result,"Ada","secret",true));
        assert(current.authenticated() && current.session().id()!=old);
        co_return result;
    });
    const auto token=cookie(response,"gungnir_remember"); assert(!token.empty());
    Request recall{http::Method::get,"/"}; recall.set_header("Cookie","gungnir_remember="+token);
    auto recalled=with_session(recall,[&](Request& current) -> Task<Response> {
        Response result; assert(guard.recall(current,result) && current.authenticated()); co_return result;
    });
    const auto rotated=cookie(recalled,"gungnir_remember"); assert(rotated!=token);
    Request replay{http::Method::get,"/"}; replay.set_header("Cookie","gungnir_remember="+token);
    with_session(replay,[&](Request& current) -> Task<Response> { Response result; assert(!guard.recall(current,result)); co_return result; });
    guard.logout(recall,recalled); assert(recall.guest() && recall.session().values().empty());
    Request revoked{http::Method::get,"/"}; revoked.set_header("Cookie","gungnir_remember="+rotated);
    with_session(revoked,[&](Request& current) -> Task<Response> { Response result; assert(!guard.recall(current,result)); co_return result; });
#endif
}
