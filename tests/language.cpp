#include <cassert>
#include <string>
#include <variant>

#include <gungnir/language/language.hpp>

int main() {
    const auto expression_program = gungnir::language::Parser{
        gungnir::language::Lexer{
            "controller Math { int calculate(int n) { const x = n + 2 * 3; "
            "return (x - 1); } int invalid() { return 1 + 2.5; } }"
        }.tokenize(), "expressions.gnr"
    }.parse().program;
    const auto& math = std::get<gungnir::language::FrameworkDeclaration>(
        expression_program.nodes.front());
    const auto& calculate = std::get<gungnir::language::ControllerMethod>(
        expression_program.nodes[math.members.front()]);
    assert(calculate.body.size() == 2);
    const auto& sum = calculate.body.front().expression;
    assert(sum.kind == gungnir::language::ExpressionKind::binary && sum.text == "+");
    assert(sum.arguments[1].kind == gungnir::language::ExpressionKind::binary &&
           sum.arguments[1].text == "*");
    assert(calculate.body.back().expression.kind ==
           gungnir::language::ExpressionKind::group);
    const auto list_program = gungnir::language::Parser{
        gungnir::language::Lexer{
            "controller Lists { void build() { const values = [1, add(2, 3)]; } }"
        }.tokenize(), "lists.gnr"
    }.parse().program;
    const auto& lists = std::get<gungnir::language::FrameworkDeclaration>(
        list_program.nodes.front());
    const auto& build = std::get<gungnir::language::ControllerMethod>(
        list_program.nodes[lists.members.front()]);
    assert(build.body.front().expression.kind ==
           gungnir::language::ExpressionKind::list);
    assert(build.body.front().expression.arguments.size() == 2);
    const auto lowered_lists = gungnir::language::Transpiler{}.transpile(
        "controller Lists { void build() { const numbers = [1, 2]; "
        "const nested = [[1, 2], [3, 4]]; const empty = []; } }",
        "lowered_lists.gnr", {.emit_line_directives = false}
    );
    assert(lowered_lists.success());
    assert(lowered_lists.code.find("std::vector{1, 2}") != std::string::npos);
    assert(lowered_lists.code.find("std::vector{std::vector{1, 2}, "
                                   "std::vector{3, 4}}") != std::string::npos);
    assert(lowered_lists.code.find("std::vector<gungnir::view::Value>{}") !=
           std::string::npos);
    const auto collections = gungnir::language::Transpiler{}.transpile(
        "controller Collections { void build() { "
        "const mixed = [1, \"two\", true]; "
        "const data = {\"items\": [1, 2], \"nested\": {\"ok\": true}}; "
        "const rows = [{\"id\": 1}, {\"id\": 2}]; "
        "view(\"page\", {\"items\": mixed}); } }",
        "collections.gnr", {.emit_line_directives = false}
    );
    assert(collections.success());
    assert(collections.code.find("std::vector<gungnir::view::Value>{") !=
           std::string::npos);
    assert(collections.code.find("gungnir::view::make_value(1)") !=
           std::string::npos);
    assert(collections.code.find("gungnir::view::Data{{\"items\", ") !=
           std::string::npos);
    assert(collections.code.find("gungnir::view::Data{{\"ok\", true}") !=
           std::string::npos);
    const auto invalid_object = gungnir::language::Transpiler{}.transpile(
        "controller Objects { void build() { const data = {missing: 1}; } }",
        "invalid_object.gnr", {.emit_line_directives = false}
    );
    bool invalid_object_key = false;
    for (const auto& diagnostic : invalid_object.diagnostics)
        invalid_object_key |= diagnostic.code == "GNR1014";
    assert(invalid_object_key);
    const auto loop_list = gungnir::language::Transpiler{}.transpile(
        "controller Lists { void build() { for (const values = [1, 2]; "
        "true; ) { break; } } }",
        "loop_list.gnr", {.emit_line_directives = false}
    );
    assert(loop_list.success());
    assert(loop_list.code.find("const auto values = std::vector{1, 2}") !=
           std::string::npos);
    const auto access_program = gungnir::language::Parser{
        gungnir::language::Lexer{
            "controller Access { void load() { const users = User::all(); "
            "const active = true && false; } }"
        }.tokenize(), "access.gnr"
    }.parse().program;
    const auto& access = std::get<gungnir::language::FrameworkDeclaration>(
        access_program.nodes.front());
    const auto& load = std::get<gungnir::language::ControllerMethod>(
        access_program.nodes[access.members.front()]);
    assert(load.body[0].expression.kind == gungnir::language::ExpressionKind::call);
    assert(load.body[0].expression.arguments.front().kind ==
           gungnir::language::ExpressionKind::member);
    assert(load.body[0].expression.arguments.front().text == "::");
    assert(load.body[1].expression.kind == gungnir::language::ExpressionKind::binary);
    assert(load.body[1].expression.text == "&&");
    const auto malformed_expressions = gungnir::language::Parser{
        gungnir::language::Lexer{
            "controller BadExpressions { void run() { "
            "const values = [1,,2]; const x = 1 +; const item = values[]; "
            "return view(\"page\", { \"x\" 1 }); } }"
        }.tokenize(), "bad_expressions.gnr"
    }.parse();
    bool missing_list_element = false, missing_object_colon = false;
    bool missing_operand = false, missing_subscript = false;
    for (const auto& diagnostic : malformed_expressions.diagnostics) {
        missing_list_element |= diagnostic.message == "List element is missing";
        missing_object_colon |= diagnostic.message ==
            "Object entry requires a key, ':' and value";
        missing_operand |= diagnostic.code == "GNR1012";
        missing_subscript |= diagnostic.code == "GNR1013";
    }
    assert(missing_list_element && missing_object_colon && missing_operand &&
           missing_subscript);
    const auto recovered_list = gungnir::language::Parser{
        gungnir::language::Lexer{
            "controller Recover { void run() { const bad = [1,; "
            "const good = [2, 3]; } }"
        }.tokenize(), "recover.gnr"
    }.parse();
    bool unclosed_list = false;
    for (const auto& diagnostic : recovered_list.diagnostics)
        unclosed_list |= diagnostic.code == "GNR1011";
    const auto& recovered_declaration =
        std::get<gungnir::language::FrameworkDeclaration>(
            recovered_list.program.nodes.front());
    const auto& recovered_method = std::get<gungnir::language::ControllerMethod>(
        recovered_list.program.nodes[recovered_declaration.members.front()]);
    assert(unclosed_list && recovered_method.body.size() == 2);
    assert(recovered_method.body[1].expression.kind ==
           gungnir::language::ExpressionKind::list);
    const auto directive_program = gungnir::language::Parser{
        gungnir::language::Lexer{
            "#define COUNT \\\n 2\ncontroller WithDirective { void run() { const values = [COUNT]; } }"
        }.tokenize(), "directive.gnr"
    }.parse();
    assert(directive_program.diagnostics.empty());
    assert(std::get<gungnir::language::FrameworkDeclaration>(
               directive_program.program.nodes.front()).class_name == "WithDirective");
    const auto native_postfix = gungnir::language::Parser{
        gungnir::language::Lexer{
            "controller Native { int next(int value) { return value++; } }"
        }.tokenize(), "native.gnr"
    }.parse();
    assert(native_postfix.diagnostics.empty());
    const auto expression_diagnostics = gungnir::language::SemanticAnalyzer{}.analyze(
        expression_program, "expressions.gnr");
    bool invalid_arithmetic_return = false;
    for (const auto& diagnostic : expression_diagnostics)
        invalid_arithmetic_return |= diagnostic.code == "GNR1306";
    assert(invalid_arithmetic_return);
    const auto operand_program = gungnir::language::Parser{
        gungnir::language::Lexer{
            "controller Operands { bool check() { return true && 1; } }"
        }.tokenize(), "operands.gnr"
    }.parse().program;
    bool operand_error = false;
    for (const auto& diagnostic : gungnir::language::SemanticAnalyzer{}.analyze(
             operand_program, "operands.gnr"))
        operand_error |= diagnostic.code == "GNR1316";
    assert(operand_error);

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

    bool linked_controller_method = false;
    for (const auto& node : grammar_ast.program.nodes) {
        const auto* declaration =
            std::get_if<gungnir::language::FrameworkDeclaration>(&node);
        if (!declaration || declaration->class_name != "AccountController") {
            continue;
        }
        for (const auto member_index : declaration->members) {
            const auto* method = std::get_if<gungnir::language::ControllerMethod>(
                &grammar_ast.program.nodes[member_index]
            );
            if (method && method->name == "show") {
                linked_controller_method = method->parameters.size() == 1 &&
                    method->parameters[0].type_name == "Request" &&
                    method->parameters[0].name == "request" &&
                    method->body.size() == 1 &&
                    method->body[0].kind ==
                        gungnir::language::StatementKind::return_ &&
                    method->body[0].expression.kind ==
                        gungnir::language::ExpressionKind::call;
            }
        }
    }
    assert(linked_controller_method);

    const auto nested_body = gungnir::language::Parser{
        gungnir::language::Lexer{
            "controller Nested { Response index() { "
            "if (ready) { return text(\"yes\"); } "
            "return view(\"page\", { \"ready\": ready }); } }"
        }.tokenize(), "nested.gnr"
    }.parse();
    const auto& nested_declaration =
        std::get<gungnir::language::FrameworkDeclaration>(
            nested_body.program.nodes.front()
        );
    const auto& nested_method = std::get<gungnir::language::ControllerMethod>(
        nested_body.program.nodes[nested_declaration.members.front()]
    );
    assert(nested_method.body.size() == 2);
    assert(nested_method.body[0].kind ==
           gungnir::language::StatementKind::conditional);
    assert(nested_method.body[0].children.size() == 1);
    assert(nested_method.body[0].children[0].kind ==
           gungnir::language::StatementKind::return_);
    assert(nested_method.body[1].kind ==
           gungnir::language::StatementKind::return_);
    const auto& view_call = nested_method.body[1].expression;
    assert(view_call.kind == gungnir::language::ExpressionKind::call);
    assert(view_call.arguments.size() == 3);
    assert(view_call.arguments[2].kind == gungnir::language::ExpressionKind::object);
    assert(view_call.arguments[2].arguments.size() == 1);
    assert(view_call.arguments[2].arguments[0].kind ==
           gungnir::language::ExpressionKind::entry);

    const auto control_program = gungnir::language::Parser{
        gungnir::language::Lexer{
            "controller Flow { int choose(bool ready) { "
            "if (ready) { const x = 1; return x; } "
            "else { while (false) { return 2; } return 3; } "
            "return 4; } }"
        }.tokenize(), "flow.gnr"
    }.parse().program;
    const auto& flow = std::get<gungnir::language::FrameworkDeclaration>(
        control_program.nodes.front());
    const auto& choose = std::get<gungnir::language::ControllerMethod>(
        control_program.nodes[flow.members.front()]);
    assert(choose.body.size() == 2);
    assert(choose.body[0].alternative.size() == 2);
    assert(choose.body[0].alternative[0].kind ==
           gungnir::language::StatementKind::loop_);
    assert(choose.body[0].alternative[0].children.size() == 1);
    assert(gungnir::language::SemanticAnalyzer{}.analyze(
        control_program, "flow.gnr").empty());
    const auto for_program = gungnir::language::Parser{
        gungnir::language::Lexer{
            "controller Iteration { void run() { "
            "for (const i = 0; true; i = i + 1) { break; } } }"
        }.tokenize(), "for.gnr"
    }.parse().program;
    const auto& iteration = std::get<gungnir::language::FrameworkDeclaration>(
        for_program.nodes.front());
    const auto& run = std::get<gungnir::language::ControllerMethod>(
        for_program.nodes[iteration.members.front()]);
    assert(run.body.front().for_parts.size() == 3);
    assert(run.body.front().for_parts[1].kind ==
           gungnir::language::ExpressionKind::literal);
    bool immutable_for_update = false;
    for (const auto& diagnostic : gungnir::language::SemanticAnalyzer{}.analyze(
             for_program, "for.gnr"))
        immutable_for_update |= diagnostic.code == "GNR1320";
    assert(immutable_for_update);
    const auto lowered_for = gungnir::language::Transpiler{}.transpile(
        "controller Iteration { void run() { "
        "for (const i = 0; i < 3; ) { break; } } }",
        "for_lowering.gnr", {.emit_line_directives = false}
    );
    assert(lowered_for.success());
    assert(lowered_for.code.find("for (const auto i = 0;") != std::string::npos);
    const auto invalid_for_condition = gungnir::language::Parser{
        gungnir::language::Lexer{
            "controller Loop { void run() { for (const i = 0; i; ) { "
            "i = 2; } } }"
        }.tokenize(), "invalid_for.gnr"
    }.parse().program;
    bool nonboolean_for = false, changed_for_constant = false;
    for (const auto& diagnostic : gungnir::language::SemanticAnalyzer{}.analyze(
             invalid_for_condition, "invalid_for.gnr")) {
        nonboolean_for |= diagnostic.code == "GNR1311";
        changed_for_constant |= diagnostic.code == "GNR1320";
    }
    assert(nonboolean_for && changed_for_constant);
    const auto mutable_for = gungnir::language::Transpiler{}.transpile(
        "controller Loops { void run(int existing) { "
        "for (i = 0; i < 3; i = i + 1) { break; } "
        "for (existing = 0; existing < 3; existing = existing + 1) { break; } "
        "int native = 0; for (native = 1; native < 3; ) { break; } "
        "for (j = 0; j < 2; ) { break; } } }",
        "mutable_for.gnr", {.emit_line_directives = false}
    );
    assert(mutable_for.success());
    assert(mutable_for.code.find("for (auto i = 0;") != std::string::npos);
    assert(mutable_for.code.find("for (existing = 0;") != std::string::npos);
    assert(mutable_for.code.find("for (native = 1;") != std::string::npos);
    assert(mutable_for.code.find("for (auto j = 0;") != std::string::npos);
    assert(mutable_for.code.find("; auto i =") == std::string::npos);
    assert(mutable_for.code.find("; auto existing =") == std::string::npos);
    const auto invalid_flow = gungnir::language::Parser{
        gungnir::language::Lexer{
            "controller InvalidFlow { int choose() { "
            "if (1) { return true; } else { return 2; } } }"
        }.tokenize(), "invalid_flow.gnr"
    }.parse().program;
    bool condition_error = false, nested_return_error = false;
    for (const auto& diagnostic : gungnir::language::SemanticAnalyzer{}.analyze(
             invalid_flow, "invalid_flow.gnr")) {
        condition_error |= diagnostic.code == "GNR1311";
        nested_return_error |= diagnostic.code == "GNR1306";
    }
    assert(condition_error && nested_return_error);

    const auto flow_edges = gungnir::language::Parser{
        gungnir::language::Lexer{
            "controller Edges { void okay() { while (true) { break; } return; } "
            "int missing() { return; } void wrong() { continue; } }"
        }.tokenize(), "flow_edges.gnr"
    }.parse().program;
    bool missing_value = false, invalid_continue = false;
    for (const auto& diagnostic : gungnir::language::SemanticAnalyzer{}.analyze(
             flow_edges, "flow_edges.gnr")) {
        missing_value |= diagnostic.code == "GNR1312";
        invalid_continue |= diagnostic.code == "GNR1313";
        assert(diagnostic.code != "GNR1304");
    }
    assert(missing_value && invalid_continue);

    const auto chain_program = gungnir::language::Parser{
        gungnir::language::Lexer{
            "controller Chain { int choose(bool a, bool b) { "
            "if (a) { return 1; } else if (b) { return 2; } "
            "else { return 3; } return 4; } }"
        }.tokenize(), "chain.gnr"
    }.parse().program;
    const auto& chain = std::get<gungnir::language::FrameworkDeclaration>(
        chain_program.nodes.front());
    const auto& chained_method = std::get<gungnir::language::ControllerMethod>(
        chain_program.nodes[chain.members.front()]);
    assert(chained_method.body.size() == 2);
    assert(chained_method.body[0].alternative.size() == 1);
    assert(chained_method.body[0].alternative[0].alternative.size() == 1);

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

    const auto semantic_input = gungnir::language::Parser{
        gungnir::language::Lexer{
            "controller Example {\n"
            "    void run(int value, int value) { return 1; }\n"
            "    void run(int value) {}\n"
            "    void run(int value) {}\n"
            "}\n"
        }.tokenize(), "semantic.gnr"
    }.parse();
    const auto semantic_errors = gungnir::language::SemanticAnalyzer{}.analyze(
        semantic_input.program, "semantic.gnr"
    );
    bool duplicate_parameter = false;
    bool duplicate_method = false;
    bool invalid_return = false;
    for (const auto& diagnostic : semantic_errors) {
        duplicate_parameter |= diagnostic.code == "GNR1302";
        duplicate_method |= diagnostic.code == "GNR1301";
        invalid_return |= diagnostic.code == "GNR1304";
    }
    assert(duplicate_parameter && duplicate_method && invalid_return);

    const auto invalid_semantics = gungnir::language::Transpiler{}.transpile(
        "controller Example { bool show() { return \"wrong\"; } }\n"
        "Route::get(\"/missing\", Example::missing);\n",
        "invalid_semantics.gnr",
        {.emit_line_directives = false}
    );
    bool wrong_return_type = false;
    bool missing_action = false;
    for (const auto& diagnostic : invalid_semantics.diagnostics) {
        wrong_return_type |= diagnostic.code == "GNR1306";
        missing_action |= diagnostic.code == "GNR1307";
    }
    assert(wrong_return_type && missing_action);
    const auto inferred_return = gungnir::language::Transpiler{}.transpile(
        "controller Example { bool show() { "
        "const answer = \"yes\"; return answer; } }",
        "inferred_return.gnr",
        {.emit_line_directives = false}
    );
    bool wrong_inferred_return = false;
    for (const auto& diagnostic : inferred_return.diagnostics) {
        wrong_inferred_return |= diagnostic.code == "GNR1306";
    }
    assert(wrong_inferred_return);

    const auto shared_controller = gungnir::language::Parser{
        gungnir::language::Lexer{
            "controller PostsController { Response index() {} }"
        }.tokenize(), "controllers.gnr"
    }.parse();
    const auto shared_model = gungnir::language::Parser{
        gungnir::language::Lexer{"model Post { string title; }"}.tokenize(),
        "models.gnr"
    }.parse();
    gungnir::language::SemanticIndex project_index;
    project_index.add(shared_controller.program);
    project_index.add(shared_model.program);
    project_index.closed_world = true;
    const auto route_middleware = gungnir::language::Parser{
        gungnir::language::Lexer{
            "middleware AuthMiddleware {} policy WrongMiddleware {}"
        }.tokenize(), "middleware.gnr"
    }.parse().program;
    gungnir::language::SemanticIndex middleware_index = project_index;
    middleware_index.add(route_middleware, "middleware.gnr");
    const auto middleware_routes = gungnir::language::Parser{
        gungnir::language::Lexer{
            "Route::get(\"/ok\", PostsController::index)"
            ".middleware(AuthMiddleware); "
            "Route::get(\"/wrong\", PostsController::index)"
            ".middleware(WrongMiddleware); "
            "Route::get(\"/unknown\", PostsController::index)"
            ".middleware(MissingMiddleware);"
        }.tokenize(), "middleware_routes.gnr"
    }.parse().program;
    unsigned invalid_middleware_routes = 0;
    for (const auto& diagnostic : gungnir::language::SemanticAnalyzer{}.analyze(
             middleware_routes, "middleware_routes.gnr", &middleware_index))
        invalid_middleware_routes += diagnostic.code == "GNR1322";
    assert(invalid_middleware_routes == 2);
    const auto colliding_model = gungnir::language::Parser{
        gungnir::language::Lexer{"model Post { string label; }"}.tokenize(),
        "duplicate_model.gnr"
    }.parse();
    gungnir::language::SemanticIndex duplicate_project;
    duplicate_project.add(shared_model.program, "models.gnr");
    duplicate_project.add(colliding_model.program, "duplicate_model.gnr");
    bool duplicate_project_type = false;
    for (const auto& diagnostic : gungnir::language::SemanticAnalyzer{}.analyze(
             colliding_model.program, "duplicate_model.gnr", &duplicate_project))
        duplicate_project_type |= diagnostic.code == "GNR1314";
    assert(duplicate_project_type);
    const auto same_file_declarations = gungnir::language::Parser{
        gungnir::language::Lexer{
            "model Same {} controller Same {}"
        }.tokenize(), "same_file.gnr"
    }.parse().program;
    unsigned same_file_duplicates = 0;
    for (const auto& diagnostic : gungnir::language::SemanticAnalyzer{}.analyze(
             same_file_declarations, "same_file.gnr"))
        same_file_duplicates += diagnostic.code == "GNR1314";
    assert(same_file_duplicates == 1);
    const auto through_program = gungnir::language::Parser{
        gungnir::language::Lexer{
            "model Post {} controller AccountController {} "
            "model User { posts() { return hasManyThrough<Post, "
            "AccountController>(); } }"
        }.tokenize(), "through.gnr"
    }.parse().program;
    bool invalid_through_type = false;
    for (const auto& diagnostic : gungnir::language::SemanticAnalyzer{}.analyze(
             through_program, "through.gnr"))
        invalid_through_type |= diagnostic.code == "GNR1303";
    assert(invalid_through_type);
    const auto call_source = gungnir::language::Parser{
        gungnir::language::Lexer{
            "controller Calculator { int doubleValue(int n) { return n + n; } "
            "int bad() { return doubleValue(\"wrong\"); } "
            "int missing() { return doubleValue(); } }"
        }.tokenize(), "calls.gnr"
    }.parse().program;
    bool wrong_argument = false, missing_argument = false;
    for (const auto& diagnostic : gungnir::language::SemanticAnalyzer{}.analyze(
             call_source, "calls.gnr")) {
        wrong_argument |= diagnostic.code == "GNR1318";
        missing_argument |= diagnostic.code == "GNR1317";
    }
    assert(wrong_argument && missing_argument);
    const auto nested_calls = gungnir::language::Parser{
        gungnir::language::Lexer{
            "controller NestedCalls { int take(int n) { return n; } "
            "void run() { const values = [take(\"wrong\")]; "
            "const item = values[take(\"wrong\")]; "
            "view(\"page\", {\"items\": [take(\"wrong\")]}); } }"
        }.tokenize(), "nested_calls.gnr"
    }.parse().program;
    unsigned nested_argument_errors = 0;
    for (const auto& diagnostic : gungnir::language::SemanticAnalyzer{}.analyze(
             nested_calls, "nested_calls.gnr"))
        nested_argument_errors += diagnostic.code == "GNR1318";
    assert(nested_argument_errors == 3);
    const auto invalid_index = gungnir::language::Parser{
        gungnir::language::Lexer{
            "controller Index { void run() { const values = [1, 2]; "
            "const item = values[\"first\"]; } }"
        }.tokenize(), "invalid_index.gnr"
    }.parse().program;
    bool noninteger_index = false;
    for (const auto& diagnostic : gungnir::language::SemanticAnalyzer{}.analyze(
             invalid_index, "invalid_index.gnr"))
        noninteger_index |= diagnostic.code == "GNR1321";
    assert(noninteger_index);
    const auto named_types = gungnir::language::Parser{
        gungnir::language::Lexer{
            "model User {} model Post {} "
            "controller Types { User identity(User user) { return user; } "
            "User wrong(Post post) { return post; } "
            "User call(Post post) { return identity(post); } }"
        }.tokenize(), "named_types.gnr"
    }.parse().program;
    bool wrong_named_return = false, wrong_named_argument = false;
    for (const auto& diagnostic : gungnir::language::SemanticAnalyzer{}.analyze(
             named_types, "named_types.gnr")) {
        wrong_named_return |= diagnostic.code == "GNR1306";
        wrong_named_argument |= diagnostic.code == "GNR1318";
    }
    assert(wrong_named_return && wrong_named_argument);
    const auto external_models = gungnir::language::Parser{
        gungnir::language::Lexer{"model User {} model Post {}"}.tokenize(),
        "external_models.gnr"
    }.parse().program;
    const auto external_controller = gungnir::language::Parser{
        gungnir::language::Lexer{
            "controller Types { User wrong(Post post) { return post; } }"
        }.tokenize(), "external_controller.gnr"
    }.parse().program;
    gungnir::language::SemanticIndex named_index;
    named_index.add(external_models, "external_models.gnr");
    bool wrong_cross_file_return = false;
    for (const auto& diagnostic : gungnir::language::SemanticAnalyzer{}.analyze(
             external_controller, "external_controller.gnr", &named_index))
        wrong_cross_file_return |= diagnostic.code == "GNR1306";
    assert(wrong_cross_file_return);
    const auto return_call = gungnir::language::Parser{
        gungnir::language::Lexer{
            "controller Calculator { int value() { return 2; } "
            "bool invalid() { return value(); } }"
        }.tokenize(), "return_call.gnr"
    }.parse().program;
    bool invalid_call_return = false;
    for (const auto& diagnostic : gungnir::language::SemanticAnalyzer{}.analyze(
             return_call, "return_call.gnr"))
        invalid_call_return |= diagnostic.code == "GNR1306";
    assert(invalid_call_return);
    const auto scoped_program = gungnir::language::Parser{
        gungnir::language::Lexer{
            "controller Scopes { "
            "int shadow(int value) { if (true) { const value = \"inner\"; "
            "const value = \"again\"; } return value; } "
            "int duplicate(int value) { const value = 1; return value; } "
            "int separate() { if (true) { const item = 1; } "
            "if (true) { const item = 2; } return 1; } }"
        }.tokenize(), "scopes.gnr"
    }.parse().program;
    unsigned duplicate_locals = 0;
    bool invalid_shadow_return = false;
    for (const auto& diagnostic : gungnir::language::SemanticAnalyzer{}.analyze(
             scoped_program, "scopes.gnr")) {
        duplicate_locals += diagnostic.code == "GNR1310";
        invalid_shadow_return |= diagnostic.code == "GNR1306";
    }
    assert(duplicate_locals == 2 && !invalid_shadow_return);
    const auto return_paths = gungnir::language::Parser{
        gungnir::language::Lexer{
            "controller Paths { "
            "int missing(bool flag) { if (flag) { return 1; } } "
            "int complete(bool flag) { if (flag) { return 1; } "
            "else { return 2; } } "
            "int trailing(bool flag) { if (flag) { return 1; } return 2; } "
            "int loop(bool flag) { while (flag) { return 1; } } }"
        }.tokenize(), "paths.gnr"
    }.parse().program;
    unsigned missing_paths = 0;
    for (const auto& diagnostic : gungnir::language::SemanticAnalyzer{}.analyze(
             return_paths, "paths.gnr"))
        missing_paths += diagnostic.code == "GNR1319";
    assert(missing_paths == 2);
    const auto constant_assignments = gungnir::language::Parser{
        gungnir::language::Lexer{
            "controller Constants { void run(int mutableValue) { "
            "const value = 1; value = 2; "
            "if (true) { value = 3; const inner = 1; inner = 2; } "
            "mutableValue = 4; } }"
        }.tokenize(), "constants.gnr"
    }.parse().program;
    unsigned immutable_assignments = 0;
    for (const auto& diagnostic : gungnir::language::SemanticAnalyzer{}.analyze(
             constant_assignments, "constants.gnr"))
        immutable_assignments += diagnostic.code == "GNR1320";
    assert(immutable_assignments == 3);
    const auto helper_program = gungnir::language::Parser{
        gungnir::language::Lexer{
            "controller Helper { int calculate(int n) { return n; } }"
        }.tokenize(), "helper.gnr"
    }.parse().program;
    const auto caller_program = gungnir::language::Parser{
        gungnir::language::Lexer{
            "controller Caller { int run() { return Helper::calculate(3); } }"
        }.tokenize(), "caller.gnr"
    }.parse().program;
    gungnir::language::SemanticIndex call_index;
    call_index.add(helper_program, "helper.gnr");
    call_index.add(caller_program, "caller.gnr");
    assert(gungnir::language::SemanticAnalyzer{}.analyze(
        caller_program, "caller.gnr", &call_index).empty());
    const auto cross_file_route = gungnir::language::Transpiler{}.transpile(
        "Route::get(\"/posts\", PostsController::index);",
        "routes.gnr",
        {.emit_line_directives = false, .semantic_index = &project_index}
    );
    assert(cross_file_route.success());
    const auto missing_cross_file_action =
        gungnir::language::Transpiler{}.transpile(
        "Route::get(\"/posts\", PostsController::missing);",
        "routes.gnr",
        {.emit_line_directives = false, .semantic_index = &project_index}
    );
    assert(!missing_cross_file_action.success());
    bool missing_project_action = false;
    for (const auto& diagnostic : missing_cross_file_action.diagnostics) {
        missing_project_action |= diagnostic.code == "GNR1307";
    }
    assert(missing_project_action);
    const auto cross_file_relationship =
        gungnir::language::Transpiler{}.transpile(
            "model User { posts() { return hasMany<Post>(); } }",
            "users.gnr",
            {.emit_line_directives = false, .semantic_index = &project_index}
        );
    assert(cross_file_relationship.success());
    const auto missing_related_model =
        gungnir::language::Transpiler{}.transpile(
            "model User { posts() { return hasMany<MissingPost>(); } }",
            "users.gnr",
            {.emit_line_directives = false, .semantic_index = &project_index}
        );
    bool unknown_related_type = false;
    for (const auto& diagnostic : missing_related_model.diagnostics) {
        unknown_related_type |= diagnostic.code == "GNR1308";
    }
    assert(unknown_related_type);

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
            "class UserPolicy : public gungnir::Policy {\npublic:\n};"
        ) != std::string::npos
    );
    assert(
        artifact_declarations.code.find(
            "class UserCreated : public gungnir::Event {\npublic:\n};"
        ) != std::string::npos
    );
    assert(
        artifact_declarations.code.find(
            "class SendWelcome : public gungnir::Listener {\npublic:\n};"
        ) != std::string::npos
    );
    assert(
        artifact_declarations.code.find(
            "class WelcomeNotification : public gungnir::Notification {\npublic:\n};"
        ) != std::string::npos
    );
    assert(
        artifact_declarations.code.find(
            "class WelcomeMail : public gungnir::Mail {\npublic:\n};"
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
    const auto nested_async = transpiler.transpile(
        "controller NestedAsync { async Response show(bool ready) { "
        "if (ready) { return await load(); } "
        "return text(\"wait\"); } }",
        "nested_async.gnr", {.emit_line_directives = false}
    );
    assert(nested_async.success());
    assert(nested_async.code.find("co_return co_await load();") !=
           std::string::npos);

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
    const auto terminated_migration = transpiler.transpile(
        "migration Existing {} /* kept */ ;\n",
        "terminated_migration.gnr",
        {.emit_line_directives = false}
    );
    assert(terminated_migration.success());
    assert(terminated_migration.code.find("} /* kept */ ;") != std::string::npos);

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
    const auto commented_view = transpiler.transpile(
        "controller Comments { Response show() { "
        "return view(\"page\", { \"x\" /* : , */ : 1, \"y\": 2 }); } }",
        "commented_view.gnr", {.emit_line_directives = false}
    );
    assert(commented_view.success());
    assert(commented_view.code.find("/* : , */ , 1}") != std::string::npos);
    assert(
        view_data.code.find("users.count()") !=
        std::string::npos
    );

    return 0;
}
