#include "structured_validation_program.cpp"
#include "fixtures/structured/upload_samples.hpp"
#include <gungnir/language/compiler.hpp>
#include <gungnir/language/cpp_ir.hpp>
#include <gungnir/http/files.hpp>
#include <gungnir/storage/local_disk.hpp>
#include <gungnir/database/sqlite.hpp>

#include <chrono>
#include <coroutine>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <locale>

using namespace gungnir;
#define CHECK(...) do { if (!(__VA_ARGS__)) { std::cerr << __FILE__ << ":" << __LINE__ << ": " << #__VA_ARGS__ << std::endl; std::exit(EXIT_FAILURE); } } while (false)
namespace {
void stage(std::string_view name) { std::cerr << "validation: " << name << std::endl; }
void reject(std::string_view body, std::string_view prelude = "") {
    const String source = String{prelude} + "controller Invalid { inject Storage storage; index(Request request) { " + String{body} + " } }";
    const auto emitted = language::Compiler{}.compile(source, "invalid-validation.gnr");
    language::CompilerOptions options; options.validate_only = true;
    const auto checked = language::Compiler{}.compile(source, "invalid-validation.gnr", options);
    CHECK(!emitted.success() && !checked.success() && emitted.code.empty() && checked.code.empty());
    CHECK(emitted.diagnostics.size() == checked.diagnostics.size());
    for (std::size_t i = 0; i < emitted.diagnostics.size(); ++i) {
        const auto& a = emitted.diagnostics[i]; const auto& b = checked.diagnostics[i];
        CHECK(a.code == b.code && a.message == b.message && a.span.valid && b.span.valid);
        CHECK(a.location.line == b.location.line && a.location.column == b.location.column);
        CHECK(a.span.begin_offset == b.span.begin_offset && a.span.end_offset == b.span.end_offset);
    }
}
String unhex(std::string_view hex) {
    String bytes; for (std::size_t i = 0; i < hex.size(); i += 2) bytes += static_cast<char>(std::stoi(String{hex.substr(i, 2)}, nullptr, 16)); return bytes;
}
String part(String field, String bytes, String filename = {}, String type = "application/octet-stream") {
    String header = "--GungnirBoundary\r\nContent-Disposition: form-data; name=\"" + field + "\"";
    if (!filename.empty()) header += "; filename=\"" + filename + "\"\r\nContent-Type: " + type;
    return header + "\r\n\r\n" + bytes + "\r\n";
}
constexpr auto multipart_type = "multipart/form-data; boundary=\"GungnirBoundary\"";
String finish(String body) { return body + "--GungnirBoundary--\r\n"; }
template<class Exception, class F> void throws(F action) { bool caught{}; try { action(); } catch (const Exception&) { caught = true; } CHECK(caught); }
bool passes(Json value, String expression) { return validation::Validator::check(Json::object({{"value", std::move(value)}}), {{"value", std::move(expression)}}).valid(); }
struct DecimalComma : std::numpunct<char> { char do_decimal_point() const override { return ','; } };
Task<void> dispatch(Router& router, Request& request, std::optional<Response>& response) { response = co_await router.dispatch(request); }
Task<int> suspended_dispatch(Router& router, Request& request, std::optional<Response>& response) { co_await dispatch(router,request,response); co_return 0; }
template<class T> void bind(Application& app) { app.bind<T>([](Container& container) { if constexpr (requires { T::make(container); }) return T::make(container); else return std::make_shared<T>(); }); }
Response send(Router& router, String path, String body, String type = "application/json") {
    Request request{http::Method::post, std::move(path), std::move(body)}; request.set_header("Content-Type", std::move(type)); request.set_header("Accept", "application/json");
    std::optional<Response> response; language::runtime::wait(dispatch(router, request, response)); CHECK(response); return std::move(*response);
}
struct Files {
    std::filesystem::path root = std::filesystem::temp_directory_path() / ("gungnir-validation-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ~Files() { std::error_code error; std::filesystem::remove_all(root, error); }
};
struct Pause {
    std::coroutine_handle<>* handle;
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> value) const noexcept { *handle = value; }
    void await_resume() const noexcept {}
};
}
int main() {
    stage("canonical diagnostics");
    for (const String rule : {"mystery", "sometimes|unknown", "min", "min:nan", "min:inf", "min:2,3", "length:-1", "length:1.5", "required:yes", "same", "same:a..b", "requiredIf:type", "unique:bad-table,email", "unique:users,bad column", "exists:users,id,ignore", "mimes:exe", "custom:bad-name", "string||max:4", ""}) reject("return json(request.validate({\"value\": \"" + rule + "\"}));");
    for (const auto definition : {"1", "true", "null", "[]", "{ value: 1 }", "{ value: {} }", "{ 'users.x*': 'string' }"}) reject(String{"return json(request.validate("} + definition + "));" );
    for (const String rule : {"length:9007199254740992.1", "length:18446744073709551616", "mimetypes:/", "mimetypes:image/", "mimetypes:image/png/extra", "mimetypes:image/(png)"}) {
        reject("return json(request.validate({\"value\": \"" + rule + "\"}));");
        throws<std::logic_error>([&] { (void)validation::Validator::check(Json::object({}), {{"missing", "sometimes|" + rule}}); });
    }
    reject("return json(request.validate({value: 'custom:project'}));");
    reject("return json(Validator::validate({}, {value: 'custom:project'}));");
    reject("return json(request.file('x').bytes());");
    reject("return json(request.files());");
    reject("return json(request.check({value: 'string'}));");
    reject("return json(request.hasFile(1));");
    reject("return json(request.validate({}, 1));");
    reject("const v = Validator::make(); v.extend('a', 'message', (int value, Json data) => true); return noContent();");
    reject("const v = Validator::make(); v.extend('a', 'message', (Json value, Json data) => value); return noContent();");
    reject("const v = Validator::make(); v.extend('a', 'message', predicate); return noContent();", "async function bool predicate(Json value, Json data) { return true; }");
    reject("const v = Validator::make(); v.extend('a', 'message', predicate); return noContent();", "function bool predicate(Json value) { return true; }");
    reject("const v = Validator::make(); v.extend('bad-name', 'message', (Json value, Json data) => true); return noContent();");
    reject("return json(Validator);");
    reject("const bad = UploadedFile; return noContent();");
    reject("const bad = ValidationResult; return noContent();");
    const auto compiled = language::Compiler{}.compile("function Json data(Json value) { return Validator::validate(value, { id: 'required|uuid' }); }", "ir.gnr");
    CHECK(compiled.success());
    CHECK(language::CppIrVerifier{}.verify(language::CppIrLowerer{}.lower(*compiled.validated, false)).success());

    stage("rules and JSON types");
    CHECK(passes(Json{std::numeric_limits<UInt64>::max()}, "integer|max:18446744073709551615"));
    CHECK(!passes(Json{std::numeric_limits<UInt64>::max()}, "integer|max:18446744073709551614"));
    CHECK(passes(Json{std::numeric_limits<Int64>::min()}, "integer|min:-9223372036854775808"));
    CHECK(!passes(Json{std::numeric_limits<Int64>::min()}, "integer|min:-9223372036854775807"));
    CHECK(passes(Json{"-1.25"}, "numeric|min:-1.5|max:0.25"));
    CHECK(passes(Json{"0.003"}, "numeric|min:2e-3|max:3e-3"));
    CHECK(passes(Json{0.3}, "numeric|min:0.3|max:0.3"));
    CHECK(passes(Json{"0e2147483647"}, "numeric|min:0|max:0"));
    CHECK(!passes(Json{"18446744073709551616"}, "integer"));
    CHECK(!passes(Json{1.0}, "integer") && !passes(Json{true}, "integer") && !passes(Json{true}, "numeric"));
    CHECK(passes(Json{false}, "boolean") && passes(Json{"off"}, "boolean") && !passes(Json{"maybe"}, "boolean"));
    throws<std::invalid_argument>([] { (void)Json{std::numeric_limits<double>::infinity()}; });
    CHECK(!passes(Json{1.0}, "accepted") && passes(Json{true}, "accepted"));
    CHECK(passes(Json{"a+b@example.test"}, "email"));
    for (const auto email : {"a b@example.test", "a@-example.test", "a..b@example.test", "@example.test", "a@example..test"}) CHECK(!passes(Json{email}, "email"));
    CHECK(passes(Json{"550e8400-e29b-41d4-a716-446655440000"}, "uuid") && passes(Json{"00000000-0000-0000-0000-000000000000"}, "uuid"));
    CHECK(!passes(Json{"550e8400-e29b-41d4-a716-44665544000z"}, "uuid"));
    for (const auto url : {"https://example.test/a%20b?q=1#part", "HTTP://localhost:80/", "http://127.0.0.1/", "https://[2001:db8::1]:443/", "http://[::ffff:192.0.2.1]/"}) CHECK(passes(Json{url}, "url"));
    for (const auto url : {"/relative", "ftp://example.test", "https://", "https://user:pass@example.test", "https://example.test:65536/", "http://999.0.0.1/", "http://[:::1]/", "http://[::1]tail/", "http://example.test/space here", "http://example.test/%zz", "http://example.test/#a#b"}) CHECK(!passes(Json{url}, "url"));
    CHECK(passes(Json{"2000-02-29"}, "date") && !passes(Json{"1900-02-29"}, "date"));
    for (const auto date : {"2025-02-29", "2024-04-31", "0000-01-01", "2024-1-01", "2024-01-00"}) CHECK(!passes(Json{date}, "date"));
    CHECK(passes(Json{"2024-02-29T23:59:59.123+05:30"}, "datetime"));
    CHECK(!passes(Json{"2024-02-29T24:00:00Z"}, "datetime") && !passes(Json{"2024-02-29T00:00:00"}, "datetime"));
    auto types = validation::Validator::check(Json::parse(R"({"a":true,"b":"true","c":42,"d":42})"), {{"a","same:b"},{"c","same:d"}});
    CHECK(types.errors.contains("a") && !types.errors.contains("c"));
    CHECK(passes(Json::array({Json{"a"},Json{"b"}}), "array|length:2|min:2|max:2"));
    auto bail = validation::Validator::check(Json::object({{"x",Json::array({})}}), {{"x","bail|array|min:1|length:2"}}); CHECK(bail.errors.at("x").size() == 1);
    for (const auto expression : {"sometimes|min:bad", "sometimes|unique:bad table,email", "sometimes|unknown"}) throws<std::logic_error>([&] { (void)validation::Validator::check(Json::object({}), {{"missing",expression}}); });
    auto present = validation::Validator::check(Json::object({}), {{"missing","present"}}); CHECK(present.errors.at("missing")[0] == "The missing field must be present.");
    CHECK(validation::Validator::check(Json::parse(R"({"x":null})"), {{"x","present"}}).valid());
    auto flat = validation::Validator::check(validation::Input{{"x",""}}, {{"x","nullable|integer"}}); CHECK(flat.valid() && flat.values.at("x").empty());
    const auto old_locale = std::locale::global(std::locale{std::locale::classic(), new DecimalComma});
    CHECK(passes(Json{1.25}, "numeric|min:1.2|max:1.3")); std::locale::global(old_locale);

    stage("projection and conditions");
    auto nested = validation::Validator::check(Json::parse(R"({"profile":{"email":"a@example.test","admin":true},"users":[{"mode":"email","email":"b@example.test","confirmation":"b@example.test","admin":true}]})"), {{"profile","object"},{"profile.email","email"},{"users","array"},{"users.*.email","required|email"},{"users.*.confirmation","same:users.*.email"},{"users.*.delivery","requiredIf:users.*.mode,email|string"}});
    CHECK(nested.errors.contains("users.0.delivery"));
    CHECK(!nested.values.get("profile")->get("admin") && !nested.values.get("users")->as_array()[0].get("admin"));
    auto conditional = validation::Validator::check(Json::parse(R"({"kind":"email"})"), {{"email","requiredIf:kind,email"}}); CHECK(!conditional.valid());
    CHECK(validation::Validator::check(Json::parse(R"({"kind":"sms"})"), {{"email","requiredIf:kind,email"}}).valid());

    stage("registry ownership and isolation");
    auto registry = domainRules("GNR"); CHECK(registry.validate(Json::parse(R"({"code":"GNR"})"), {{"code","custom:projectCode"}}).get("code"));
    const auto shared = registry; CHECK(shared.check(Json::parse(R"({"code":"bad"})"), {{"code","custom:projectCode"}}).errors.at("code")[0] == "The code must identify this project.");
    throws<std::logic_error>([&] { (void)validation::Engine{}.check(Json::object({}), {{"missing","sometimes|custom:projectCode"}}); });
    throws<std::logic_error>([&] { registry.extend("projectCode","message",[](const auto&,const auto&) { return true; }); });
    registry.extend("context", "Context differs.", [](const Json& value, const Json& input) { return input.get("expected") && value == *input.get("expected"); });
    CHECK(registry.check(Json::parse(R"({"x":"yes","expected":"yes"})"), {{"x","custom:context"}}).valid());
    CHECK(validatePayload(Json::object({{"count",Json{std::numeric_limits<UInt64>::max()}}})).get("count")->is_integer());
    CHECK(checkPayload(Json::parse(R"({"count":true})")).failed());

    stage("multipart parsing and limits");
    constexpr char binary_data[] = "one\0two\r\n--GungnirBoundaryExtra\r\nend";
    const String binary{binary_data, sizeof(binary_data) - 1};
    const auto body = finish(part("caption","Report") + part("attachment",binary,"../report.bin"));
    Request upload{http::Method::post,"/?caption=query",body}; upload.set_header("Content-Type",multipart_type);
    CHECK(upload.form().at("caption") == "Report" && upload.files().size() == 1 && upload.has_file("attachment"));
    CHECK(upload.file("attachment")->filename == "report.bin" && upload.file("attachment")->bytes() == binary);
    CHECK(upload.validate({{"attachment","required|file|max:4096"}}).contains("attachment"));
    const auto retained = upload.file("attachment"); upload.set_header("X-Reset","yes"); CHECK(retained->bytes() == binary);
    CHECK(upload.check_structured({{"attachment","file|size:" + std::to_string(binary.size()) + "|extensions:bin"}}).valid());
    CHECK(!validation::Validator::check(Json::parse(R"({"attachment":{"name":"report.bin","size":1}})"), {{"attachment","file"}}).valid());
    for (const String broken : {body.substr(0,body.size()-5), String{"--GungnirBoundary\n\nwrong"}, finish(part("x","a") + part("x","b","x.bin")), finish(String{"--GungnirBoundary\r\nContent-Disposition: form-data; name=\"x\"; name=\"y\"\r\n\r\nx\r\n"})}) {
        Request request{http::Method::post,"/",broken}; request.set_header("Content-Type",multipart_type);
        throws<http::BadRequestException>([&] { (void)request.form(); }); throws<http::BadRequestException>([&] { (void)request.files(); });
    }
    for (const auto header : {"multipart/form-data", "multipart/form-data; boundary=", "multipart/form-data; boundary=GungnirBoundary; boundary=Other"}) throws<http::BadRequestException>([&] { (void)http::parse_multipart(header,body); });
    for (int choice = 0; choice < 5; ++choice) {
        http::MultipartLimits limits;
        if (choice == 0) limits.body_bytes = 1; if (choice == 1) limits.file_bytes = 1; if (choice == 2) limits.field_bytes = 1; if (choice == 3) limits.header_bytes = 1; if (choice == 4) limits.parts = 1;
        bool caught{}; try { (void)http::parse_multipart(multipart_type, body, limits); } catch (const http::HttpException& error) { caught = error.status() == 413; } CHECK(caught);
    }
    std::optional<http::UploadedFile> independent;
    { Request request{http::Method::post,"/",body}; request.set_header("Content-Type",multipart_type); independent = request.file("attachment"); }
    CHECK(independent && independent->bytes() == binary);

    stage("image content recognition");
    for (auto hex : validation_samples::images) {
        const auto image = unhex(hex); CHECK(http::detected_media_type(image).starts_with("image/"));
        for (std::size_t length = 0; length < image.size(); ++length) CHECK(!http::detected_media_type(std::string_view{image}.substr(0,length)).starts_with("image/"));
        Request request{http::Method::post,"/",finish(part("avatar",image,"fake.txt","text/plain"))}; request.set_header("Content-Type",multipart_type);
        CHECK(request.check_structured({{"avatar","file|image|mimetypes:image/png,image/jpeg,image/gif,image/webp"}}).valid());
    }
    Request forged{http::Method::post,"/",finish(part("avatar","not an image","face.png","image/png"))}; forged.set_header("Content-Type",multipart_type);
    CHECK(!forged.check_structured({{"avatar","file|image|mimes:png"}}).valid());
    auto corrupt = unhex(validation_samples::png); corrupt[29] ^= 1; CHECK(!http::detected_media_type(corrupt).starts_with("image/"));
    CHECK(http::detected_media_type("%PDF-1.7\n1 0 obj\nendobj\n%%EOF\r\n") == "application/pdf");
    CHECK(http::detected_media_type("plain text\n") == "text/plain");

    stage("bound database validation");
    for (const auto backend : {database::Backend::sqlite,database::Backend::mysql,database::Backend::postgresql,database::Backend::mssql}) {
        String statement; std::vector<model::AttributeValue> bindings;
        auto driver = std::make_shared<database::CallbackDriver>(backend,[&](const String& sql,const auto& values) { statement=sql; bindings=values; database::Result result; result.rows.push_back({{"matches",UInt64{0}}}); return result; });
        auto connection = std::make_shared<database::Connection>("default",driver); auto scope = database::runtime::activate(connection);
        const String hostile = "x' OR 1=1 --";
        CHECK(validation::Validator::check(Json::object({{"email",Json{hostile}}}), {{"email","unique:users,email,7,id"}}).valid());
        CHECK(bindings.size() == 2 && std::get<String>(bindings[0]) == hostile && statement.find(hostile) == String::npos);
        CHECK(statement.find(backend == database::Backend::postgresql ? "$1" : "?") != String::npos);
        const auto quoted = backend == database::Backend::mysql ? "`users`" : "\"users\""; CHECK(statement.find(quoted) != String::npos);
        CHECK(!validation::Validator::check(Json::parse(R"({"id":7})"), {{"id","exists:users,id"}}).valid());
        CHECK(bindings.size() == 1 && std::get<UInt64>(bindings[0]) == 7);
    }

    stage("generated application");
    Files files; auto manager = std::make_shared<storage::Manager>(); manager->add("local",std::make_shared<storage::LocalDisk>(files.root));
    Application app; app.provider<ServicesProvider>(ServiceOptions{.storage=manager}); bind<ValidationController>(app); bind<UploadAudit>(app); bind<InjectedValidation>(app);
    app.container().instance<validation::Engine>(std::make_shared<validation::Engine>(registry));
#ifdef GUNGNIR_WITH_SQLITE
    database::Settings settings; settings.backend=database::Backend::sqlite; settings.database=":memory:";
    app.database("default",settings.backend,[settings] { return std::make_shared<database::SQLiteDriver>(settings); },1);
#endif
    app.boot(); gnr_register_routes(app);
    const auto profile = Json::parse(R"({"enabled":false,"count":18446744073709551615,"price":-1.25,"profile":{"email":"a@example.test","admin":true},"users":[{"mode":"email","delivery":"mail","email":"b@example.test","confirmation":"b@example.test","admin":true}],"public_id":"550e8400-e29b-41d4-a716-446655440000","website":"https://example.test/a%20b","birthday":"2024-02-29","published":"2024-02-29T23:59:59.123+05:30","role":"writer","password":"secret123","password_confirmation":"secret123","empty":null,"secret":"omit"})");
    auto shown = send(app.router(),"/validation/profile",profile.dump()); CHECK(shown.status() == 200);
    const auto selected_profile = Json::parse(shown.body());
    CHECK(selected_profile.get("enabled")->is_boolean() && *selected_profile.get("count") == Json{std::numeric_limits<UInt64>::max()});
    CHECK(!selected_profile.get("secret") && !selected_profile.get("password_confirmation") && !selected_profile.get("profile")->get("admin"));
    CHECK(!selected_profile.get("users")->as_array()[0].get("admin") && !selected_profile.get("users")->as_array()[0].get("mode"));
    auto bad_profile = profile.as_object(); bad_profile["count"] = Json{true}; bad_profile["website"] = Json{"invalid"}; bad_profile["birthday"] = Json{"2025-02-29"};
    CHECK(send(app.router(),"/validation/profile",Json::object(std::move(bad_profile)).dump()).status() == 422);
    auto response = send(app.router(),"/validation/check",R"({"email":"bad","count":3,"secret":"omit"})"); CHECK(response.status() == 200);
    auto report = Json::parse(response.body()); CHECK(report.get("failed")->string() == "true" && report.get("errors")->get("email")->as_array().size() == 1 && !report.get("values")->get("secret"));
    for (const auto path : {"/validation/custom","/validation/custom-data","/validation/injected"}) {
        CHECK(send(app.router(),path,R"({"code":"GNR"})").status() == 200);
        const auto invalid = send(app.router(),path,R"({"code":"bad"})"); CHECK(invalid.status() == 422 && invalid.body().find("bad") == String::npos);
    }
    CHECK(send(app.router(),"/validation/custom-check",R"({"code":"bad"})").status() == 200);
    for (const auto json : {"", "{bad", "[]"}) CHECK(send(app.router(),"/validation/check",json).status() == 400);
    response = send(app.router(),"/validation/upload",body,multipart_type); CHECK(response.status() == 200 && response.header("X-Upload-Name") == "report.bin" && response.header("X-Validation-Registry") == "valid");
    CHECK(manager->disk("local").get("uploads/attachment.bin") == binary);
    CHECK(send(app.router(),"/validation/upload",finish(part("caption","Report")+part("attachment",String(4097,'x'),"large.bin")),multipart_type).status() == 422);
    CHECK(manager->disk("local").get("uploads/attachment.bin") == binary);
    const auto png = unhex(validation_samples::png);
    CHECK(send(app.router(),"/validation/image",finish(part("avatar",png,"avatar.png")),multipart_type).status() == 200);
    CHECK(send(app.router(),"/validation/image",finish(part("avatar","forged","avatar.png","image/png")),multipart_type).status() == 422);
    CHECK(send(app.router(),"/validation/image",R"({"avatar":{"name":"avatar.png","size":2}})").status() == 422);
    response = send(app.router(),"/validation/multiple",finish(part("photos[]",png,"a.png")+part("photos[]",png,"b.png")),multipart_type); CHECK(response.status() == 200 && Json::parse(response.body()).get("count")->string() == "2");
    CHECK(send(app.router(),"/validation/multiple",finish(part("photos[]",png,"a.png")+part("photos[]","forged","b.png")),multipart_type).status() == 422);
    CHECK(send(app.router(),"/validation/upload",body.substr(0,body.size()-5),multipart_type).status() == 400);
#ifdef GUNGNIR_WITH_SQLITE
    app.database().connection()->execute("CREATE TABLE validation_users (id INTEGER PRIMARY KEY, email TEXT UNIQUE)");
    app.database().connection()->execute("INSERT INTO validation_users (id,email) VALUES (?,?)", {Int64{7},String{"a@example.test"}});
    CHECK(send(app.router(),"/validation/unique",R"({"email":"a@example.test"})").status() == 422);
    CHECK(send(app.router(),"/validation/unique",R"({"email":"b@example.test"})").status() == 200);
    CHECK(send(app.router(),"/validation/exists",R"({"id":7})").status() == 200 && send(app.router(),"/validation/exists",R"({"id":8})").status() == 422);
#endif

    stage("ownership through suspension");
    std::coroutine_handle<> paused;
    auto delayed = app.router().post("/validation/delayed", [](Request&) -> Task<Response> { co_return Response::text("resumed"); });
    delayed.middleware<UploadAudit>();
    delayed.middleware([&](Request& request,http::Next next) -> Task<Response> { co_await Pause{&paused}; co_return co_await next(request); });
    Request request{http::Method::post,"/validation/delayed",body}; request.set_header("Content-Type",multipart_type);
    std::optional<Response> resumed; auto task = suspended_dispatch(app.router(),request,resumed); task.run_inline(); CHECK(paused && !task.done());
    request.set_header("Content-Type","application/octet-stream"); paused.resume(); CHECK(task.done() && resumed);
    CHECK(task.operator co_await().await_resume() == 0);
    CHECK(resumed->header("X-Upload-Name") == "report.bin" && resumed->header("X-Validation-Registry") == "valid");
    stage("complete");
}
