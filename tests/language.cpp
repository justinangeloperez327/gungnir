#include <cassert>
#include <string>

#include <gungnir/language/language.hpp>

int main() {
    gungnir::language::Transpiler transpiler;

    const auto immutable = transpiler.transpile(
        "const users = User::all();\n",
        "immutable.gnr",
        {.emit_line_directives = false}
    );

    assert(immutable.success());
    assert(immutable.code == "const auto users = User::all();\n");

    const auto mutable_binding = transpiler.transpile(
        "void load() {\n"
        "    users = User::all();\n"
        "    users = User::where(\"active\", true).get();\n"
        "}\n",
        "mutable.gnr",
        {.emit_line_directives = false}
    );

    assert(mutable_binding.success());
    assert(
        mutable_binding.code.find("auto users = User::all();") !=
        std::string::npos
    );
    assert(
        mutable_binding.code.find(
            "auto users = User::where"
        ) == std::string::npos
    );

    const auto nested_scope = transpiler.transpile(
        "void load() {\n"
        "    value = 1;\n"
        "    {\n"
        "        value = 2;\n"
        "        const other = 3;\n"
        "    }\n"
        "}\n",
        "scope.gnr",
        {.emit_line_directives = false}
    );

    assert(nested_scope.success());
    assert(nested_scope.code.find("auto value = 1;") != std::string::npos);
    assert(nested_scope.code.find("auto value = 2;") == std::string::npos);
    assert(nested_scope.code.find("const auto other = 3;") != std::string::npos);

    const auto framework_classes = transpiler.transpile(
        "class User : Model {};\n"
        "class UserController : Controller {};\n"
        "class CreateUsers : Migration {};\n",
        "classes.gnr",
        {.emit_line_directives = false}
    );

    assert(framework_classes.success());
    assert(
        framework_classes.code.find(
            "class User : public gungnir::Model<User>"
        ) != std::string::npos
    );
    assert(
        framework_classes.code.find(
            "class UserController : public gungnir::Controller"
        ) != std::string::npos
    );
    assert(
        framework_classes.code.find(
            "class CreateUsers : public gungnir::Migration"
        ) != std::string::npos
    );

    const auto native_cpp = transpiler.transpile(
        "void load() {\n"
        "    const int count = 10;\n"
        "    User user = make_user();\n"
        "    count_value = count;\n"
        "}\n",
        "native.gnr",
        {.emit_line_directives = false}
    );

    assert(native_cpp.success());
    assert(
        native_cpp.code.find("const int count = 10;") !=
        std::string::npos
    );
    assert(
        native_cpp.code.find("User user = make_user();") !=
        std::string::npos
    );
    assert(
        native_cpp.code.find("auto count_value = count;") !=
        std::string::npos
    );

    const auto strings_and_comments = transpiler.transpile(
        "void load() {\n"
        "    const text = \"user = fake;\";\n"
        "    // fake = value;\n"
        "    real = 1;\n"
        "}\n",
        "tokens.gnr",
        {.emit_line_directives = false}
    );

    assert(strings_and_comments.success());
    assert(
        strings_and_comments.code.find(
            "const auto text = \"user = fake;\";"
        ) != std::string::npos
    );
    assert(
        strings_and_comments.code.find("// fake = value;") !=
        std::string::npos
    );
    assert(
        strings_and_comments.code.find("auto real = 1;") !=
        std::string::npos
    );

    const auto mapped = transpiler.transpile(
        "const user = User::find(1);\n",
        "app/controllers/user_controller.gnr"
    );

    assert(mapped.success());
    assert(
        mapped.code.starts_with(
            "#line 1 \"app/controllers/user_controller.gnr\"\n"
        )
    );

    const auto model = transpiler.transpile(
        "class User : Model {\n"
        "    string name;\n"
        "    string email;\n"
        "    string? nickname;\n"
        "    bool active = true;\n"
        "\n"
        "    posts() {\n"
        "        return hasMany<Post>();\n"
        "    }\n"
        "}\n",
        "user.gnr",
        {.emit_line_directives = false}
    );

    assert(model.success());
    assert(
        model.code.find(
            "class User : public gungnir::Model<User>"
        ) != std::string::npos
    );
    assert(
        model.code.find(
            "inline static constexpr gungnir::Table table{\"users\"};"
        ) != std::string::npos
    );
    assert(
        model.code.find(
            "gungnir::PrimaryKey<gungnir::Integer> id;"
        ) != std::string::npos
    );
    assert(
        model.code.find(
            "gungnir::Field<gungnir::String> name;"
        ) != std::string::npos
    );
    assert(
        model.code.find(
            "gungnir::Field<std::optional<gungnir::String>> nickname;"
        ) != std::string::npos
    );
    assert(
        model.code.find(
            "gungnir::HasMany<Post> __gungnir_relation_posts{\"user_id\", \"id\"};"
        ) != std::string::npos
    );
    assert(
        model.code.find(
            "gungnir::model::attribute(\"name\", &User::name)"
        ) != std::string::npos
    );
    assert(
        model.code.find(
            "gungnir::model::relation(\"posts\", &User::__gungnir_relation_posts)"
        ) != std::string::npos
    );

    const auto belongs_to = transpiler.transpile(
        "class Post : Model {\n"
        "    int userId;\n"
        "    string title;\n"
        "    user() { return belongsTo<User>(); }\n"
        "}\n",
        "post.gnr",
        {.emit_line_directives = false}
    );

    assert(belongs_to.success());
    assert(
        belongs_to.code.find(
            "gungnir::ForeignKey<User, gungnir::Integer> userId;"
        ) != std::string::npos
    );
    assert(
        belongs_to.code.find(
            "gungnir::BelongsTo<User> __gungnir_relation_user{\"user_id\", \"id\"};"
        ) != std::string::npos
    );

    const auto configured = transpiler.transpile(
        "class AuditUser : Model {\n"
        "    table = \"legacy_users\";\n"
        "    connection = \"reporting\";\n"
        "    timestamps = false;\n"
        "    softDeletes = true;\n"
        "    string name;\n"
        "}\n",
        "configured.gnr",
        {.emit_line_directives = false}
    );

    assert(configured.success());
    assert(
        configured.code.find(
            "gungnir::Table table{\"legacy_users\"}"
        ) != std::string::npos
    );
    assert(
        configured.code.find(
            "gungnir::Connection connection{\"reporting\"}"
        ) != std::string::npos
    );
    assert(
        configured.code.find(
            "gungnir::SoftDeletes soft_deletes{\"deleted_at\"}"
        ) != std::string::npos
    );
    assert(configured.code.find("createdAt") == std::string::npos);
    assert(configured.code.find("updatedAt") == std::string::npos);

    const auto eloquent_names = transpiler.transpile(
        "void load() {\n"
        "    const user = User::findOrFail(1);\n"
        "    const users = User::whereIn(\"id\", ids).orderBy(\"name\").get();\n"
        "    user.delete();\n"
        "}\n",
        "eloquent.gnr",
        {.emit_line_directives = false}
    );

    assert(eloquent_names.success());
    assert(
        eloquent_names.code.find("User::find_or_fail(1)") !=
        std::string::npos
    );
    assert(
        eloquent_names.code.find("User::where_in(\"id\", ids)") !=
        std::string::npos
    );
    assert(
        eloquent_names.code.find(".order_by(\"name\")") !=
        std::string::npos
    );
    assert(
        eloquent_names.code.find("user.remove()") !=
        std::string::npos
    );

    const auto controller = transpiler.transpile(
        "class UserController : Controller {\n"
        "    inject Clock clock;\n"
        "\n"
        "    Response index() {\n"
        "        const name = clock.name();\n"
        "        return text(name);\n"
        "    }\n"
        "}\n"
        "\n"
        "Route::get(\"/users\", UserController::index);\n"
        "Route::delete(\"/users/{id}\", UserController::index);\n",
        "controller.gnr",
        {.emit_line_directives = false}
    );

    assert(controller.success());
    assert(
        controller.code.find(
            "class UserController : public gungnir::Controller"
        ) != std::string::npos
    );
    assert(
        controller.code.find(
            "explicit UserController(gungnir::Container& __gungnir_container)"
        ) != std::string::npos
    );
    assert(
        controller.code.find(
            "std::shared_ptr<Clock> clock;"
        ) != std::string::npos
    );
    assert(
        controller.code.find(
            "clock->name()"
        ) != std::string::npos
    );
    assert(
        controller.code.find(
            "gungnir::Route::get<UserController>(\"/users\", &UserController::index)"
        ) != std::string::npos
    );
    assert(
        controller.code.find(
            "gungnir::Route::remove<UserController>(\"/users/{id}\", &UserController::index)"
        ) != std::string::npos
    );

    const auto async_controller = transpiler.transpile(
        "class AsyncController : Controller {\n"
        "    async Response index() {\n"
        "        const result = await load_response();\n"
        "        return result;\n"
        "    }\n"
        "\n"
        "    async Response show(Request request) {\n"
        "        return text(request.parameter(\"id\"));\n"
        "    }\n"
        "}\n",
        "async_controller.gnr",
        {.emit_line_directives = false}
    );

    assert(async_controller.success());
    assert(
        async_controller.code.find(
            "gungnir::Task<gungnir::Response> index()"
        ) != std::string::npos
    );
    assert(
        async_controller.code.find(
            "const auto result = co_await load_response();"
        ) != std::string::npos
    );
    assert(
        async_controller.code.find(
            "co_return result;"
        ) != std::string::npos
    );
    assert(
        async_controller.code.find(
            "gungnir::Task<gungnir::Response> show(gungnir::Request& request)"
        ) != std::string::npos
    );
    assert(
        async_controller.code.find(
            "co_return text(request.parameter(\"id\"));"
        ) != std::string::npos
    );

    const auto sync_request = transpiler.transpile(
        "class RequestController : Controller {\n"
        "    Response show(Request request) {\n"
        "        return text(request.parameter(\"id\"));\n"
        "    }\n"
        "}\n",
        "request_controller.gnr",
        {.emit_line_directives = false}
    );

    assert(sync_request.success());
    assert(
        sync_request.code.find(
            "gungnir::Response show(gungnir::Request& request)"
        ) != std::string::npos
    );

    const auto middleware = transpiler.transpile(
        "class AuthMiddleware : Middleware {\n"
        "    async Response handle(Request request, Next next) {\n"
        "        if (request.header(\"authorization\").empty()) {\n"
        "            return text(\"Unauthorized\", 401);\n"
        "        }\n"
        "        return await next(request);\n"
        "    }\n"
        "}\n"
        "Route::get(\"/users\", UserController::index)"
        ".middleware(AuthMiddleware);\n",
        "middleware.gnr",
        {.emit_line_directives = false}
    );

    assert(middleware.success());
    assert(
        middleware.code.find(
            "class AuthMiddleware : public gungnir::Middleware"
        ) != std::string::npos
    );
    assert(
        middleware.code.find(
            "gungnir::Next next"
        ) != std::string::npos
    );
    assert(
        middleware.code.find(
            ".middleware<AuthMiddleware>()"
        ) != std::string::npos
    );

    const auto validation = transpiler.transpile(
        "class UserController : Controller {\n"
        "    Response store(Request request) {\n"
        "        const data = request.validate({\n"
        "            \"name\": \"required|string|max:80\",\n"
        "            \"email\": \"required|email\"\n"
        "        });\n"
        "        return json(data);\n"
        "    }\n"
        "}\n",
        "validation.gnr",
        {.emit_line_directives = false}
    );

    assert(validation.success());
    assert(
        validation.code.find(
            "gungnir::validation::Rules{"
        ) != std::string::npos
    );
    assert(
        validation.code.find(
            "{\"name\", \"required|string|max:80\"}"
        ) != std::string::npos
    );
    assert(
        validation.code.find(
            "{\"email\", \"required|email\"}"
        ) != std::string::npos
    );

    const auto bootstrap = transpiler.transpile(
        "int main() {\n"
        "    app = Application::create();\n"
        "    app.run();\n"
        "}\n",
        "main.gnr",
        {.emit_line_directives = false}
    );

    assert(bootstrap.success());
    assert(
        bootstrap.code.find(
            "auto app = gungnir::Application::create();"
        ) != std::string::npos
    );
    assert(
        bootstrap.code.find(
            "app.run();"
        ) != std::string::npos
    );

    const auto invalid_await = transpiler.transpile(
        "void load() {\n"
        "    const value = await fetch();\n"
        "}\n",
        "invalid_await.gnr",
        {.emit_line_directives = false}
    );

    assert(!invalid_await.success());
    assert(!invalid_await.diagnostics.empty());

    const auto view_data = transpiler.transpile(
        "class PageController : Controller {\n"
        "    Response index() {\n"
        "        const users = User::all();\n"
        "        return view(\"users/index\", {\n"
        "            \"users\": users,\n"
        "            \"title\": \"Users\",\n"
        "            \"count\": users.count()\n"
        "        });\n"
        "    }\n"
        "}\n",
        "view_controller.gnr",
        {.emit_line_directives = false}
    );

    assert(view_data.success());
    assert(
        view_data.code.find(
            "gungnir::view::Data{"
        ) != std::string::npos
    );
    assert(
        view_data.code.find(
            "{\"users\", users}"
        ) != std::string::npos
    );
    assert(
        view_data.code.find(
            "{\"title\", \"Users\"}"
        ) != std::string::npos
    );
    assert(
        view_data.code.find("\"count\"") !=
        std::string::npos
    );
    assert(
        view_data.code.find("users.count()") !=
        std::string::npos
    );

    return 0;
}
