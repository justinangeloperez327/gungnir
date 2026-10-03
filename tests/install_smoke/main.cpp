#include <string>

#include <gungnir/gungnir.hpp>
#include <gungnir/language/compiler.hpp>
#include <gungnir/orm/orm.hpp>
#include <gungnir/version.hpp>

static_assert(
    gungnir::version == "1.0.0-rc.1"
);

static_assert(
    gungnir::native_api_contract_version ==
        "1.0"
);

static_assert(
    gungnir::native_abi_epoch == 1
);

int main() {
    auto application =
        gungnir::Application::create();

    application.http_runtime(
        gungnir::http::RuntimeOptions{}
    );

    auto response =
        gungnir::http::Response::text(
            "installed-package"
        );

    if (
        response.status() != 200 ||
        response.body() !=
            "installed-package"
    ) {
        return 1;
    }

    gungnir::language::CompilerOptions
        compiler_options;

    compiler_options.emit_line_directives =
        false;

    const auto compiled =
        gungnir::language::Compiler{}
            .compile(
                "function int answer() { return 42; }",
                "install-smoke.gnr",
                compiler_options
            );

    if (!compiled.success()) {
        return 2;
    }

    gungnir::orm::QueryPlan plan;
    plan.table = "users";
    plan.limit = 1;

    const auto query =
        gungnir::orm::compile(
            plan,
            gungnir::database::Backend::
                postgresql
        );

    if (
        query.text.find(
            "FROM \"users\""
        ) == std::string::npos
    ) {
        return 3;
    }

    return 0;
}
