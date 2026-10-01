#include <gungnir/database/sqlite.hpp>
#include <gungnir/database/error.hpp>
#include <sqlite3.h>
#include <limits>
#include <mutex>
#include <cctype>

namespace gungnir::database {
class SQLiteDriver::Impl {
public:
    sqlite3* connection{};
    std::mutex mutex;
    ~Impl() { if (connection) sqlite3_close_v2(connection); }
    void check(int code) const {
        if (code != SQLITE_OK) throw Error{sqlite3_errmsg(connection), Backend::sqlite};
    }
};
SQLiteDriver::SQLiteDriver(const Settings& settings) : impl_(std::make_unique<Impl>()) {
    if (settings.database.empty() || settings.database.find('\0') != String::npos)
        throw std::invalid_argument("SQLite requires a database path or :memory:");
    impl_->check(sqlite3_open_v2(settings.database.c_str(), &impl_->connection,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr));
    impl_->check(sqlite3_busy_timeout(impl_->connection, 5000));
    execute("PRAGMA foreign_keys = ON");
}
SQLiteDriver::~SQLiteDriver() = default;
Result SQLiteDriver::execute(const String& sql, const std::vector<model::AttributeValue>& bindings) {
    if (sql.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()) || sql.find('\0') != String::npos)
        throw std::invalid_argument("Invalid SQLite statement length");
    std::lock_guard lock{impl_->mutex};
    sqlite3_stmt* raw{};
    const char* tail{};
    impl_->check(sqlite3_prepare_v2(impl_->connection, sql.data(), static_cast<int>(sql.size()), &raw, &tail));
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement{raw, sqlite3_finalize};
    if (!raw) throw std::invalid_argument("Empty SQLite statement");
    while (tail < sql.data() + sql.size() && std::isspace(static_cast<unsigned char>(*tail))) ++tail;
    if (tail != sql.data() + sql.size()) throw std::invalid_argument("SQLite accepts one statement per call");
    if (bindings.size() != static_cast<std::size_t>(sqlite3_bind_parameter_count(raw)))
        throw std::invalid_argument("SQLite binding count does not match statement");
    for (std::size_t i = 0; i < bindings.size(); ++i) {
        const auto index = static_cast<int>(i + 1);
        const auto code = std::visit([&](const auto& value) -> int {
            using T = std::remove_cvref_t<decltype(value)>;
            if constexpr (std::same_as<T, std::monostate> || std::same_as<T, std::nullptr_t>) return sqlite3_bind_null(raw, index);
            else if constexpr (std::same_as<T, UInt64>) {
                if (!std::in_range<sqlite3_int64>(value)) throw std::out_of_range("SQLite integer exceeds signed 64-bit range");
                return sqlite3_bind_int64(raw, index, static_cast<sqlite3_int64>(value));
            } else if constexpr (std::integral<T>) return sqlite3_bind_int64(raw, index, static_cast<sqlite3_int64>(value));
            else if constexpr (std::floating_point<T>) return sqlite3_bind_double(raw, index, value);
            else {
                const auto& text = [&]() -> const String& { if constexpr (std::same_as<T, model::Decimal>) return value.string(); else return value; }();
                return sqlite3_bind_text64(raw, index, text.data(), text.size(), SQLITE_TRANSIENT, SQLITE_UTF8);
            }
        }, bindings[i]);
        impl_->check(code);
    }
    Result result;
    int code{};
    while ((code = sqlite3_step(raw)) == SQLITE_ROW) {
        model::AttributeMap row;
        for (int i = 0; i < sqlite3_column_count(raw); ++i) {
            model::AttributeValue value;
            switch (sqlite3_column_type(raw, i)) {
            case SQLITE_NULL: value = nullptr; break;
            case SQLITE_INTEGER: value = static_cast<Int64>(sqlite3_column_int64(raw, i)); break;
            case SQLITE_FLOAT: value = sqlite3_column_double(raw, i); break;
            default: {
                auto* bytes = static_cast<const char*>(sqlite3_column_blob(raw, i));
                value = String{bytes ? bytes : "", static_cast<std::size_t>(sqlite3_column_bytes(raw, i))};
                break;
            }
            }
            row.insert_or_assign(sqlite3_column_name(raw, i), std::move(value));
        }
        result.rows.push_back(std::move(row));
    }
    if (code != SQLITE_DONE) impl_->check(code);
    if (!sqlite3_stmt_readonly(raw)) {
        // sqlite3_changes is unchanged by DDL, so only report DML changes.
        auto first = sql.find_first_not_of(" \t\r\n");
        String prefix = first == String::npos ? "" : sql.substr(first, 7);
        for (auto& c : prefix) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (prefix.starts_with("insert") || prefix.starts_with("replace") || prefix.starts_with("update") || prefix.starts_with("delete"))
            result.affected_rows = static_cast<std::size_t>(sqlite3_changes64(impl_->connection));
        if ((prefix.starts_with("insert") || prefix.starts_with("replace")) && result.affected_rows)
            result.inserted_id = static_cast<Int64>(sqlite3_last_insert_rowid(impl_->connection));
    }
    return result;
}
void SQLiteDriver::begin() { execute("BEGIN"); }
void SQLiteDriver::commit() { execute("COMMIT"); }
void SQLiteDriver::rollback() { execute("ROLLBACK"); }
void SQLiteDriver::cancel() noexcept { sqlite3_interrupt(impl_->connection); }
bool SQLiteDriver::ping() { try { execute("SELECT 1"); return true; } catch (...) { return false; } }
}
