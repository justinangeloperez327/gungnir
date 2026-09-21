#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <gungnir/cli/project.hpp>

namespace {

std::string read(
    const std::filesystem::path& path
) {
    std::ifstream input{
        path,
        std::ios::binary
    };

    return std::string{
        std::istreambuf_iterator<char>{
            input
        },
        std::istreambuf_iterator<char>{}
    };
}

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        path_ =
            std::filesystem::temp_directory_path() /
            "gungnir-cli-test";

        std::filesystem::remove_all(
            path_
        );

        std::filesystem::create_directories(
            path_
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
    using gungnir::cli::Project;

    TemporaryDirectory temporary;

    const auto destination =
        temporary.path() /
        "sample-app";

    auto project =
        Project::create(
            destination,
            "sample-app"
        );

    assert(
        std::filesystem::exists(
            destination /
            ".gungnir-project"
        )
    );

    assert(
        std::filesystem::exists(
            destination /
            "app/controllers/"
            "home_controller.gnr"
        )
    );

    assert(
        std::filesystem::exists(
            destination /
            "routes/web.gnr"
        )
    );

    assert(
        std::filesystem::exists(
            destination /
            "views/welcome.html"
        )
    );

    assert(
        std::filesystem::exists(
            destination /
            ".env.example"
        )
    );

    const auto model =
        project.make_model("User");

    const auto controller =
        project.make_controller(
            "User"
        );

    const auto middleware =
        project.make_middleware(
            "Auth"
        );

    const auto migration =
        project.make_migration(
            "create_users_table"
        );

    assert(
        model.filename() ==
        "user.gnr"
    );

    assert(
        controller.filename() ==
        "user_controller.gnr"
    );

    assert(
        middleware.filename() ==
        "auth_middleware.gnr"
    );

    assert(
        migration.filename() ==
        "create_users_table.gnr"
    );

    assert(
        read(model).find(
            "class User : Model"
        ) != std::string::npos
    );

    assert(
        read(controller).find(
            "class UserController : Controller"
        ) != std::string::npos
    );

    assert(
        read(middleware).find(
            "class AuthMiddleware : Middleware"
        ) != std::string::npos
    );

    assert(
        read(migration).find(
            "Table::create(\"users\""
        ) != std::string::npos
    );

    const auto generated =
        project.assemble();

    const auto source =
        read(generated);

    const auto migration_source =
        read(
            destination /
            ".gungnir/generated/migrations.cpp"
        );

    assert(
        source.find(
            "class User : public "
            "gungnir::Model<User>"
        ) != std::string::npos
    );

    assert(
        source.find(
            "class HomeController : public "
            "gungnir::Controller"
        ) != std::string::npos
    );

    assert(
        source.find(
            "gungnir::Route::get"
            "<HomeController>"
        ) != std::string::npos
    );

    assert(
        source.find(
            "auto app = "
            "gungnir::Application::create();"
        ) != std::string::npos
    );

    assert(
        source.find(
            "app.run();"
        ) != std::string::npos
    );

    assert(
        migration_source.find(
            "CreateUsersTable migration_0"
        ) != std::string::npos
    );

    assert(
        migration_source.find(
            "migrate:plan"
        ) != std::string::npos
    );

    assert(
        migration_source.find(
            "gungnir::migration::Runner"
        ) != std::string::npos
    );

    auto nested =
        Project::open(
            destination /
            "app/controllers"
        );

    assert(
        nested.root() ==
        project.root()
    );

    bool duplicate_failed = false;

    try {
        (void) project.make_model(
            "User"
        );
    } catch (
        const std::runtime_error&
    ) {
        duplicate_failed = true;
    }

    assert(duplicate_failed);

    return 0;
}
