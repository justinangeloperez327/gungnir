#include <gungnir/database/database.hpp>
#include <gungnir/http/json.hpp>
#include <gungnir/orm/orm.hpp>
#include <cassert>
#include <memory>
#include <optional>
#include <regex>
#include <tuple>
#ifdef GUNGNIR_WITH_SQLITE
#include <gungnir/database/sqlite.hpp>
#endif

class RegressionUser : public gungnir::Model<RegressionUser> {
public:
    inline static constexpr gungnir::Table table{"users"};
    inline static constexpr auto fillable = gungnir::Fillable{"name"};
    inline static constexpr gungnir::SoftDeletes soft_deletes{"deleted_at"};
    gungnir::PrimaryKey<gungnir::Int64> id;
    gungnir::Field<gungnir::String> name;
    gungnir::Field<std::optional<gungnir::String>> deleted_at;
};
template <> struct gungnir::model::Generated<RegressionUser> {
    inline static constexpr auto attributes = std::tuple{
        gungnir::model::attribute("id",&RegressionUser::id),
        gungnir::model::attribute("name",&RegressionUser::name),
        gungnir::model::attribute("deleted_at",&RegressionUser::deleted_at)};
    inline static constexpr auto relations = std::tuple{};
};

namespace {
using namespace gungnir;

void timestamp_formats_are_checked() {
    assert(std::regex_match(model::timestamp_now("Y-m-d H:i:s.u"),
        std::regex{R"(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{6})"}));
    assert(std::regex_match(model::timestamp_now("y/m/d H:i:s.v"),
        std::regex{R"(\d{2}/\d{2}/\d{2} \d{2}:\d{2}:\d{2}\.\d{3})"}));
    assert(std::regex_match(model::timestamp_now("U"),std::regex{R"(\d+)"}));
    assert(model::timestamp_now("\\Y\\m\\d") == "Ymd");
    for (const auto* format : {"", "Y-m-d\\", "unknown"}) {
        bool rejected = false;
        try { static_cast<void>(model::timestamp_now(format)); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }
}

void native_field_operators_track_initialization() {
    Field<Int64> value;
    assert(value++ == 0 && value.initialized() && value.dirty());
    value += 3; value -= 1; value *= 2; value /= 3;
    assert(value.get() == 2);
    assert(value-- == 2 && (--value).get() == 0);
    assert((++value).get() == 1);
    value.reset();
    assert(!value.initialized() && !value.dirty());
}

void identifiers_are_escaped_in_reads_and_mutations() {
    const model::AttributeMap values{{"na\"me",String{"bound value"}}};
    const auto read = RegressionUser::where("na\"me",String{"bound value"}).compile(database::Backend::sqlite);
    assert(read.text.find("\"na\"\"me\"") != String::npos && read.bindings.size() == 1);
    assert(orm::compile_update("users",values,"id",Int64{1},database::Backend::postgresql).text.find("\"na\"\"me\"") != String::npos);
    assert(orm::compile_insert("u`ser",{{"na`me",String{"value"}}},database::Backend::mysql).text.find("`na``me`") != String::npos);
    assert(orm::compile_insert("u]ser",{{"na]me",String{"value"}}},database::Backend::mssql).text.find("[na]]me]") != String::npos);
    const auto bulk = orm::compile_update_where(RegressionUser::query().plan(),values,database::Backend::sqlite);
    assert(bulk.text.find("\"na\"\"me\"") != String::npos);
    const auto mongo = orm::compile_insert("quoted\"collection",values,database::Backend::mongodb);
    const auto json = http::Json::parse(mongo.text);
    assert(json.get("insert")->string() == "quoted\"collection");
    const auto mongo_update = orm::compile_update("users",values,"id",Int64{1},database::Backend::mongodb);
    assert(http::Json::parse(mongo_update.text).is_object());
    const auto plan = RegressionUser::query().plan();
    const auto increment = http::Json::parse(orm::compile_increment_where(plan,"na\"me",1,database::Backend::mongodb).text);
    assert(increment.get("updates")->as_array()[0].get("u")->get("$inc")->get("na\"me"));
    for (const bool restore : {false,true}) {
        const auto soft_delete = http::Json::parse(orm::compile_soft_delete_where(plan,"na\"me",restore,database::Backend::mongodb).text);
        assert(soft_delete.get("updates")->as_array()[0].get("u")->get(restore ? "$set" : "$currentDate")->get("na\"me"));
    }
    assert(http::Json::parse(orm::compile_insert_many("quoted\"collection",{values},database::Backend::mongodb).text).get("insert")->string() == "quoted\"collection");
    assert(http::Json::parse(orm::compile_upsert("quoted\"collection",{values},{"na\"me"},{},database::Backend::mongodb).text).get("update")->string() == "quoted\"collection");
    for (const auto backend : {database::Backend::sqlite,database::Backend::postgresql,database::Backend::mysql,database::Backend::mssql,database::Backend::mongodb}) {
        bool rejected = false;
        try { static_cast<void>(RegressionUser::where(String{"bad\0column",10},Int64{1}).compile(backend)); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }
    for (const auto* column : {"$comment", "nested.$where", "nested..field"}) {
        bool rejected = false;
        try { static_cast<void>(RegressionUser::where(column,String{"value"}).compile(database::Backend::mongodb)); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
        rejected = false;
        try { static_cast<void>(orm::compile_increment_where(plan,column,1,database::Backend::mongodb)); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }
    assert(orm::compile_insert("app.users",{{"name",String{"value"}}},database::Backend::postgresql).text.starts_with("INSERT INTO \"app\".\"users\""));
}

void automatic_scopes_apply_to_every_or_branch() {
    const auto query = RegressionUser::where("name",String{"a"}).or_where("name",String{"b"});
    for (const auto backend : {database::Backend::sqlite,database::Backend::postgresql,database::Backend::mysql,database::Backend::mssql}) {
        assert(query.compile(backend).text.find(" AND (") != String::npos);
        assert(orm::compile_update_where(query.plan(),{{"name",String{"changed"}}},backend).text.find(" AND (") != String::npos);
        assert(orm::compile_delete_where(query.plan(),backend).text.find(" AND (") != String::npos);
    }
    const auto mongo = http::Json::parse(query.compile(database::Backend::mongodb).text);
    assert(mongo.get("filter")->get("$and"));
    assert(http::Json::parse(orm::compile_update_where(query.plan(),{{"name",String{"changed"}}},database::Backend::mongodb).text).is_object());
    const auto unrestricted = RegressionUser::with_deleted().where("name",String{"a"}).or_where("name",String{"b"}).compile(database::Backend::sqlite);
    assert(unrestricted.text.find("deleted_at") == String::npos);
}

void empty_inserts_keep_generated_key_clauses() {
    assert(orm::compile_insert("users",{},database::Backend::sqlite,"id").text == "INSERT INTO \"users\" DEFAULT VALUES;");
    assert(orm::compile_insert("users",{},database::Backend::postgresql,"id").text == "INSERT INTO \"users\" DEFAULT VALUES RETURNING \"id\" AS \"id\";");
    assert(orm::compile_insert("users",{},database::Backend::mssql,"id").text == "INSERT INTO [users] OUTPUT INSERTED.[id] AS [id] DEFAULT VALUES;");
    assert(orm::compile_insert("users",{},database::Backend::mysql,"id").text == "INSERT INTO `users` () VALUES ();");
}

void failed_helpers_and_missing_generated_ids_are_reported() {
    database::Manager manager;
    manager.add("default",database::Backend::postgresql,[] {
        return std::make_shared<database::CallbackDriver>(database::Backend::postgresql,[](const String& sql,const auto&) {
            if (sql.starts_with("SELECT")) return database::Result{.rows={{{"id",Int64{1}},{"name",String{"before"}}}}};
            return database::Result{.affected_rows=sql.starts_with("INSERT") ? 1U : 0U};
        });
    });
    database::runtime::use(manager);
    bool rejected = false;
    try { static_cast<void>(RegressionUser::update_or_create({{"id",Int64{1}}},{{"name",String{"after"}}})); }
    catch (const std::runtime_error&) { rejected = true; }
    assert(rejected);
    RegressionUser missing;
    missing.name = "inserted";
    rejected = false;
    try { missing.save(); } catch (const ModelMetadataError&) { rejected = true; }
    assert(rejected && !missing.exists() && missing.dirty());
    database::runtime::clear();
}

void decimal_aggregate_results_are_supported() {
    database::Manager manager;
    manager.add("default",database::Backend::postgresql,[] {
        return std::make_shared<database::CallbackDriver>(database::Backend::postgresql,[](const auto&,const auto&) {
            return database::Result{.rows={{{"aggregate",model::Decimal{"10.50"}}}}};
        });
    });
    database::runtime::use(manager);
    assert(RegressionUser::query().sum("amount") == 10.5);
    assert(RegressionUser::query().average("amount") == 10.5);
    database::runtime::clear();
    assert(orm::detail::same_value(Int64{1},UInt64{1}));
    assert(orm::detail::same_value(UInt64{1},Int64{1}));
    assert(!orm::detail::same_value(Int64{-1},UInt64{1}));
}

#ifdef GUNGNIR_WITH_SQLITE
void sqlite_protects_scopes_and_identifiers_during_execution() {
    database::Settings settings;
    settings.database = ":memory:";
    settings.backend = database::Backend::sqlite;
    database::Manager manager;
    manager.add("default",settings.backend,[&] { return std::make_shared<database::SQLiteDriver>(settings); });
    database::runtime::use(manager);
    auto connection = manager.connection();
    auto scope = database::runtime::activate(connection);
    connection->execute("CREATE TABLE users(id INTEGER PRIMARY KEY, name TEXT, deleted_at TEXT)");
    connection->execute("INSERT INTO users VALUES (1,'a',NULL),(2,'b','deleted'),(3,'c',NULL)");
    const auto query = RegressionUser::where("name",String{"a"}).or_where("name",String{"b"});
    assert(query.get().size() == 1);
    assert(query.update({{"name",String{"changed"}}}) == 1);
    assert(RegressionUser::with_deleted().where_key(Int64{2}).first()->name.get() == "b");
    const auto deleted = RegressionUser::only_deleted().where("name",String{"b"}).or_where("name",String{"c"}).get();
    assert(deleted.size() == 1 && deleted[0].id.get() == 2);
    bool rejected = false;
    try {
        const auto rows = RegressionUser::where("name\" IS NOT NULL OR \"name",String{"missing"}).get();
        rejected = rows.empty();
    } catch (const database::Error&) { rejected = true; }
    assert(rejected);
    assert(RegressionUser::where("name",String{"changed"}).or_where("name",String{"b"}).force_remove() == 1);
    assert(RegressionUser::with_deleted().count() == 2);
    database::runtime::clear();
}
#endif
}

int main() {
    timestamp_formats_are_checked();
    native_field_operators_track_initialization();
    identifiers_are_escaped_in_reads_and_mutations();
    automatic_scopes_apply_to_every_or_branch();
    empty_inserts_keep_generated_key_clauses();
    failed_helpers_and_missing_generated_ids_are_reported();
    decimal_aggregate_results_are_supported();
#ifdef GUNGNIR_WITH_SQLITE
    sqlite_protects_scopes_and_identifiers_during_execution();
#endif
}
