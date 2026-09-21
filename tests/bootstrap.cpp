#include <cassert>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include <gungnir/gungnir.hpp>

namespace {

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        path_ =
            std::filesystem::temp_directory_path() /
            "gungnir-bootstrap-test";

        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(
            path_ / "templates"
        );
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(
            path_,
            error
        );
    }

    [[nodiscard]]
    const std::filesystem::path& path()
        const noexcept {
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
        std::ofstream env{
            root.path() / ".env"
        };

        env
            << "APP_NAME=\"Gungnir Test\"\n"
            << "APP_ENV=testing\n"
            << "APP_DEBUG=true\n"
            << "APP_HOST=0.0.0.0\n"
            << "APP_PORT=9123\n"
            << "VIEW_PATH=templates\n"
            << "DB_CONNECTION=postgresql\n"
            << "DB_HOST=database.internal\n"
            << "DB_PORT=5432\n"
            << "DB_DATABASE=gungnir_test\n"
            << "DB_USERNAME=tester\n";
    }

    auto app = Application::create(
        root.path()
    );

    assert(
        app.base_path() ==
        std::filesystem::absolute(root.path())
            .lexically_normal()
    );

    assert(
        app.environment() ==
        "testing"
    );

    assert(app.debug());

    assert(
        app.config().string("app.name") ==
        "Gungnir Test"
    );

    assert(
        app.config().string("server.host") ==
        "0.0.0.0"
    );

    assert(
        app.config().integer(
            "server.port"
        ) == 9123
    );

    assert(
        app.config().string(
            "database.default"
        ) == "postgresql"
    );

    assert(
        app.config().integer(
            "database.port"
        ) == 5432
    );

    assert(
        app.views().root() ==
        (
            std::filesystem::absolute(
                root.path()
            ).lexically_normal() /
            "templates"
        )
    );

    assert(
        app.env().get("APP_NAME") ==
        "Gungnir Test"
    );

    const auto config =
        app.resolve<config::Repository>();

    const auto environment =
        app.resolve<config::Environment>();

    assert(
        config.get() ==
        &app.config()
    );

    assert(
        environment.get() ==
        &app.env()
    );

    config->set("feature.enabled", true);
    config->set("feature.limit", 25);

    assert(
        config->boolean(
            "feature.enabled"
        )
    );

    assert(
        config->integer(
            "feature.limit"
        ) == 25
    );

    return 0;
}
