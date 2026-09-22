#include <cassert>
#include <filesystem>
#include <fstream>
#include <memory>

#include <gungnir/gungnir.hpp>

namespace {

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        path_ =
            std::filesystem::temp_directory_path() /
            "gungnir-database-bootstrap-test";

        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]]
    const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

} // namespace

int main() {
    using namespace gungnir;

    TemporaryDirectory root;

    {
        std::ofstream env{root.path() / ".env"};

        env
            << "DB_CONNECTION=postgresql\n"
            << "DB_NAME=default\n"
            << "DB_POOL_SIZE=3\n"
            << "DB_HOST=db.internal\n"
            << "DB_PORT=5544\n"
            << "DB_DATABASE=app\n"
            << "DB_USERNAME=user\n"
            << "DB_PASSWORD=secret\n";
    }

    auto app = Application::create(root.path());

    database::Settings captured;
    bool created = false;

    app.database_driver(
        database::Backend::postgresql,
        [&](const database::Settings& settings) {
            captured = settings;
            created = true;

            return std::make_shared<database::CallbackDriver>(
                database::Backend::postgresql,
                [](const String&, const auto&) {
                    return database::Result{};
                },
                [] {},
                [] {},
                [] {},
                [] { return true; }
            );
        }
    );

    app.boot();

    assert(app.database().has("default"));
    assert(
        app.database().backend("default") ==
        database::Backend::postgresql
    );

    auto connection = app.database().connection();

    assert(connection->healthy());
    assert(created);
    assert(captured.host == "db.internal");
    assert(captured.port == 5544);
    assert(captured.database == "app");
    assert(captured.username == "user");
    assert(captured.password == "secret");
    assert(captured.pool_size == 3);

    app.shutdown();

    auto missing = Application::create(root.path());
    bool unavailable = false;

    try {
        missing.boot();
    } catch (const database::DriverUnavailableError&) {
        unavailable = true;
    }

    assert(unavailable);

    return 0;
}
