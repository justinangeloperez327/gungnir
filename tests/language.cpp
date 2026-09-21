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

    return 0;
}
