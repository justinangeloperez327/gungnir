#include <chrono>
#include <concepts>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <gungnir/gungnir.hpp>
#include <gungnir/language/compiler.hpp>
#include <gungnir/orm/orm.hpp>

namespace {

using ApplicationCreate =
    gungnir::Application (*)(
        std::filesystem::path
    );

using ApplicationRuntimeSetter =
    gungnir::Application& (
        gungnir::Application::*
    )(
        gungnir::http::RuntimeOptions
    );

using ApplicationRuntimeGetter =
    const gungnir::http::RuntimeOptions& (
        gungnir::Application::*
    )() const noexcept;

using RouterDispatch =
    gungnir::Task<
        gungnir::http::Response
    > (
        gungnir::routing::Router::*
    )(
        gungnir::http::Request&
    ) const;

using ResponseText =
    gungnir::http::Response (*)(
        std::string,
        int
    );

using CompilerCompile =
    gungnir::language::
        CompilationResult (
            gungnir::language::Compiler::*
        )(
            std::string_view,
            std::string,
            const gungnir::language::
                CompilerOptions&
        ) const;

using OrmCompile =
    gungnir::orm::CompiledQuery (*)(
        const gungnir::orm::QueryPlan&,
        gungnir::database::Backend
    );

static_assert(
    GUNGNIR_VERSION_MAJOR == 1
);

static_assert(
    GUNGNIR_VERSION_MINOR == 0
);

static_assert(
    GUNGNIR_VERSION_PATCH == 0
);

static_assert(
    gungnir::version ==
        "1.0.0"
);

static_assert(
    gungnir::
        native_api_contract_version ==
        "1.0"
);

static_assert(
    gungnir::native_abi_epoch == 1
);

static_assert(
    std::same_as<
        gungnir::Router,
        gungnir::routing::Router
    >
);

static_assert(
    std::same_as<
        gungnir::Json,
        gungnir::http::Json
    >
);

static_assert(
    std::movable<
        gungnir::Application
    >
);

static_assert(
    !std::copy_constructible<
        gungnir::Application
    >
);

static_assert(
    std::same_as<
        decltype(
            static_cast<
                ApplicationCreate
            >(
                &gungnir::Application::
                    create
            )
        ),
        ApplicationCreate
    >
);

static_assert(
    std::same_as<
        decltype(
            static_cast<
                ApplicationRuntimeSetter
            >(
                &gungnir::Application::
                    http_runtime
            )
        ),
        ApplicationRuntimeSetter
    >
);

static_assert(
    std::same_as<
        decltype(
            static_cast<
                ApplicationRuntimeGetter
            >(
                &gungnir::Application::
                    http_runtime
            )
        ),
        ApplicationRuntimeGetter
    >
);

static_assert(
    std::same_as<
        decltype(
            &gungnir::routing::Router::
                dispatch
        ),
        RouterDispatch
    >
);

static_assert(
    std::same_as<
        decltype(
            &gungnir::http::Response::
                text
        ),
        ResponseText
    >
);

static_assert(
    std::same_as<
        decltype(
            &gungnir::language::
                Compiler::compile
        ),
        CompilerCompile
    >
);

static_assert(
    std::same_as<
        decltype(
            &gungnir::orm::compile
        ),
        OrmCompile
    >
);

static_assert(
    std::same_as<
        std::remove_cvref_t<
            decltype(
                std::declval<
                    gungnir::http::
                        RuntimeOptions
                >().max_request_bytes
            )
        >,
        std::size_t
    >
);

static_assert(
    std::same_as<
        std::remove_cvref_t<
            decltype(
                std::declval<
                    gungnir::http::
                        RuntimeOptions
                >().request_timeout
            )
        >,
        std::chrono::milliseconds
    >
);

static_assert(
    std::same_as<
        std::remove_cvref_t<
            decltype(
                std::declval<
                    gungnir::http::
                        RuntimeOptions
                >().tls
            )
        >,
        std::optional<
            gungnir::http::TlsOptions
        >
    >
);

static_assert(
    std::same_as<
        std::remove_cvref_t<
            decltype(
                std::declval<
                    gungnir::orm::
                        CompiledQuery
                >().text
            )
        >,
        gungnir::String
    >
);

} // namespace

int main() {
    gungnir::http::RuntimeOptions runtime;

    if (
        runtime.max_request_bytes !=
            1024U * 1024U ||
        runtime.max_active_dispatches !=
            1024U
    ) {
        return 1;
    }

    const auto app =
        gungnir::Application::create();

    return
        app.base_path().empty()
            ? 0
            : 0;
}
