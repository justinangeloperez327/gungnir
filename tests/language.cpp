#include <cassert>
#include <string>
#include <variant>

#include <gungnir/language/language.hpp>

int main() {
    const auto tokens =
        gungnir::language::Lexer{
            "model User { string name; }\n"
        }.tokenize();

    assert(tokens[0].kind == gungnir::language::TokenKind::keyword);
    assert(tokens[0].lexeme == "model");
    assert(tokens[2].kind == gungnir::language::TokenKind::identifier);
    assert(tokens[2].lexeme == "User");

    auto parsed =
        gungnir::language::Parser{
            tokens,
            "tokens.gnr"
        }.parse();

    assert(parsed.diagnostics.empty());

    bool found_model_declaration = false;
    for (const auto& node : parsed.program.nodes) {
        if (
            const auto* declaration =
                std::get_if<
                    gungnir::language::FrameworkDeclaration
                >(&node)
        ) {
            found_model_declaration =
                declaration->class_name == "User" &&
                declaration->kind ==
                    gungnir::language::FrameworkBaseKind::model;
        }
    }

    assert(found_model_declaration);
    const auto& model_declaration = std::get<gungnir::language::FrameworkDeclaration>(
        parsed.program.nodes.front()
    );
    assert(model_declaration.span.begin == 0);
    assert(model_declaration.span.end == std::string{"model User { string name; }"}.size());
    assert(model_declaration.members.size() == 1);
    const auto* name_field = std::get_if<gungnir::language::ModelField>(
        &parsed.program.nodes[model_declaration.members.front()]
    );
    assert(name_field && name_field->name == "name");

    const auto framework_artifacts =
        gungnir::language::Parser{
            gungnir::language::Lexer{
                "middleware AuthMiddleware {\n"
                "    async Response handle(Request request, Next next) {}\n"
                "}\n"
                "migration CreateUsersTable {\n"
                "    void up() {}\n"
                "    void down() {}\n"
                "}\n"
                "policy UserPolicy { bool view() { return true; } }\n"
                "event UserCreated { string name() { return \"user.created\"; } }\n"
                "listener SendWelcome { void handle(UserCreated event) {} }\n"
                "notification WelcomeNotification { string name() { return \"welcome\"; } }\n"
                "mail WelcomeMail { void build() {} }\n"
            }.tokenize(),
            "framework_artifacts.gnr"
        }.parse();

    assert(framework_artifacts.diagnostics.empty());

    bool found_middleware_declaration = false;
    bool found_migration_declaration = false;
    bool found_policy_declaration = false;
    bool found_event_declaration = false;
    bool found_listener_declaration = false;
    bool found_notification_declaration = false;
    bool found_mail_declaration = false;
    bool found_middleware_method = false;
    bool found_migration_method = false;
    bool found_listener_method = false;

    for (const auto& node : framework_artifacts.program.nodes) {
        if (
            const auto* declaration =
                std::get_if<
                    gungnir::language::FrameworkDeclaration
                >(&node)
        ) {
            switch (declaration->kind) {
            case gungnir::language::FrameworkBaseKind::middleware:
                found_middleware_declaration = true;
                break;
            case gungnir::language::FrameworkBaseKind::migration:
                found_migration_declaration = true;
                break;
            case gungnir::language::FrameworkBaseKind::policy:
                found_policy_declaration = true;
                break;
            case gungnir::language::FrameworkBaseKind::event:
                found_event_declaration = true;
                break;
            case gungnir::language::FrameworkBaseKind::listener:
                found_listener_declaration = true;
                break;
            case gungnir::language::FrameworkBaseKind::notification:
                found_notification_declaration = true;
                break;
            case gungnir::language::FrameworkBaseKind::mail:
                found_mail_declaration = true;
                break;
            default:
                break;
            }
        } else if (
            const auto* method =
                std::get_if<
                    gungnir::language::FrameworkMethod
                >(&node)
        ) {
            found_middleware_method =
                found_middleware_method ||
                (
                    method->owner_kind ==
                        gungnir::language::FrameworkBaseKind::middleware &&
                    method->name == "handle" &&
                    method->asynchronous
                );

            found_migration_method =
                found_migration_method ||
                (
                    method->owner_kind ==
                        gungnir::language::FrameworkBaseKind::migration &&
                    method->name == "up"
                );

            found_listener_method =
                found_listener_method ||
                (
                    method->owner_kind ==
                        gungnir::language::FrameworkBaseKind::listener &&
                    method->name == "handle"
                );
        }
    }

    assert(found_middleware_declaration);
    assert(found_migration_declaration);
    assert(found_policy_declaration);
    assert(found_event_declaration);
    assert(found_listener_declaration);
    assert(found_notification_declaration);
    assert(found_mail_declaration);
    assert(found_middleware_method);
    assert(found_migration_method);
    assert(found_listener_method);

    const auto grammar_ast =
        gungnir::language::Parser{
            gungnir::language::Lexer{
                "model Account {\n"
                "    table = \"accounts\";\n"
                "    connection = \"reporting\";\n"
                "    timestamps = false;\n"
                "    softDeletes = true;\n"
                "    string email;\n"
                "    string? nickname;\n"
                "    entries() { return hasMany<Entry>(); }\n"
                "}\n"
                "controller AccountController {\n"
                "    inject Clock clock;\n"
                "    Response index() { return text(\"ok\"); }\n"
                "    async Response show(Request request) { return text(\"ok\"); }\n"
                "}\n"
                "Route::get(\"/accounts\", AccountController::index)"
                ".middleware(AuthMiddleware);\n"
            }.tokenize(),
            "grammar_ast.gnr"
        }.parse();

    assert(grammar_ast.diagnostics.empty());

    bool found_email_field = false;
    bool found_nullable_field = false;
    bool found_table_config = false;
    bool found_timestamps_config = false;
    bool found_relationship = false;
    bool found_inject = false;
    bool found_sync_method = false;
    bool found_async_method = false;
    bool found_route = false;

    for (const auto& node : grammar_ast.program.nodes) {
        if (
            const auto* field =
                std::get_if<gungnir::language::ModelField>(&node)
        ) {
            found_email_field = found_email_field ||
                (
                    field->model_name == "Account" &&
                    field->type_name == "string" &&
                    field->name == "email" &&
                    !field->nullable
                );
            found_nullable_field = found_nullable_field ||
                (
                    field->model_name == "Account" &&
                    field->name == "nickname" &&
                    field->nullable
                );
        } else if (
            const auto* configuration =
                std::get_if<
                    gungnir::language::ModelConfiguration
                >(&node)
        ) {
            found_table_config = found_table_config ||
                (
                    configuration->model_name == "Account" &&
                    configuration->kind ==
                        gungnir::language::ModelConfigurationKind::table &&
                    configuration->value == "accounts"
                );
            found_timestamps_config = found_timestamps_config ||
                (
                    configuration->model_name == "Account" &&
                    configuration->kind ==
                        gungnir::language::ModelConfigurationKind::timestamps &&
                    !configuration->enabled
                );
        } else if (
            const auto* relationship =
                std::get_if<
                    gungnir::language::ModelRelationship
                >(&node)
        ) {
            found_relationship = found_relationship ||
                (
                    relationship->model_name == "Account" &&
                    relationship->name == "entries" &&
                    relationship->kind ==
                        gungnir::language::ModelRelationshipKind::has_many &&
                    relationship->related_type == "Entry" &&
                    relationship->through_type.empty() &&
                    relationship->arguments.empty()
                );
        } else if (
            const auto* injection =
                std::get_if<
                    gungnir::language::InjectDeclaration
                >(&node)
        ) {
            found_inject =
                injection->controller_name == "AccountController" &&
                injection->type_name == "Clock" &&
                injection->name == "clock";
        } else if (
            const auto* route =
                std::get_if<
                    gungnir::language::RouteDeclaration
                >(&node)
        ) {
            found_route = found_route ||
                (
                    route->method ==
                        gungnir::language::RouteMethodKind::get &&
                    route->controller_name == "AccountController" &&
                    route->action_name == "index" &&
                    route->has_middleware &&
                    route->middleware_type == "AuthMiddleware"
                );
        } else if (
            const auto* method =
                std::get_if<
                    gungnir::language::ControllerMethod
                >(&node)
        ) {
            found_sync_method = found_sync_method ||
                (
                    method->controller_name == "AccountController" &&
                    method->return_type == "Response" &&
                    method->name == "index" &&
                    !method->asynchronous
                );
            found_async_method = found_async_method ||
                (
                    method->controller_name == "AccountController" &&
                    method->name == "show" &&
                    method->asynchronous
                );
        }
    }

    assert(found_email_field);
    assert(found_nullable_field);
    assert(found_table_config);
    assert(found_timestamps_config);
    assert(found_relationship);
    assert(found_inject);
    assert(found_sync_method);
    assert(found_async_method);
    assert(found_route);

    const auto invalid_model =
        gungnir::language::Parser{
            gungnir::language::Lexer{
                "model { string name; }\n"
            }.tokenize(),
            "bad_model.gnr"
        }.parse();

    assert(!invalid_model.diagnostics.empty());
    assert(invalid_model.diagnostics.front().code == "GNR1001");

    const auto duplicate_declarations = gungnir::language::Parser{
        gungnir::language::Lexer{
            "model User { string name; }\n"
            "controller User { Response index() {} }\n"
        }.tokenize(),
        "duplicate.gnr"
    }.parse();
    bool found_duplicate_declaration = false;
    for (const auto& diagnostic : duplicate_declarations.diagnostics) {
        found_duplicate_declaration |= diagnostic.code == "GNR1004";
    }
    assert(found_duplicate_declaration);

    gungnir::language::Transpiler transpiler;

    const auto artifact_declarations = transpiler.transpile(
        "policy UserPolicy {}\n"
        "event UserCreated {}\n"
        "listener SendWelcome {}\n"
        "notification WelcomeNotification {}\n"
        "mail WelcomeMail {}\n",
        "artifacts.gnr",
        {.emit_line_directives = false}
    );

    assert(artifact_declarations.success());
    assert(
        artifact_declarations.code.find(
            "class UserPolicy : public gungnir::Policy {};"
        ) != std::string::npos
    );
    assert(
        artifact_declarations.code.find(
            "class UserCreated : public gungnir::Event {};"
        ) != std::string::npos
    );
    assert(
        artifact_declarations.code.find(
            "class SendWelcome : public gungnir::Listener {};"
        ) != std::string::npos
    );
    assert(
        artifact_declarations.code.find(
            "class WelcomeNotification : public gungnir::Notification {};"
        ) != std::string::npos
    );
    assert(
        artifact_declarations.code.find(
            "class WelcomeMail : public gungnir::Mail {};"
        ) != std::string::npos
    );

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
        "model User {};\n"
        "controller UserController {};\n"
        "migration CreateUsers {};\n",
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
        "model User {\n"
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
        "model Post {\n"
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
        "model AuditUser {\n"
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
        "controller UserController {\n"
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
        "controller AsyncController {\n"
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
        "controller RequestController {\n"
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
        "middleware AuthMiddleware {\n"
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
        "controller UserController {\n"
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

    const auto migration = transpiler.transpile(
        "migration CreateUsersTable {\n"
        "    void up() {}\n"
        "    void down() {}\n"
        "}\n",
        "migration.gnr",
        {.emit_line_directives = false}
    );

    assert(migration.success());
    assert(
        migration.code.find(
            "class CreateUsersTable : public gungnir::Migration"
        ) != std::string::npos
    );
    assert(
        migration.code.find(
            "public:"
        ) != std::string::npos
    );
    assert(
        migration.code.find(
            "};"
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
        "controller PageController {\n"
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
