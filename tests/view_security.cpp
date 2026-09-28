#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

#include <gungnir/view/engine.hpp>
#include <gungnir/view/error.hpp>

namespace {

void write_file(
    const std::filesystem::path& path,
    std::string_view contents
) {
    std::filesystem::create_directories(
        path.parent_path()
    );

    std::ofstream output{
        path,
        std::ios::binary |
            std::ios::trunc
    };

    assert(output);

    output.write(
        contents.data(),
        static_cast<std::streamsize>(
            contents.size()
        )
    );

    assert(output);
}

template <typename Action>
bool rejects_path(
    Action&& action
) {
    try {
        action();
    } catch (
        const gungnir::view::
            InvalidPath&
    ) {
        return true;
    }

    return false;
}

} // namespace

int main() {
    using namespace gungnir;

    const auto base =
        std::filesystem::
            temp_directory_path() /
        "gungnir-view-security-test";

    const auto root =
        base / "views";

    const auto outside =
        base / "outside";

    std::filesystem::remove_all(
        base
    );

    write_file(
        root / "safe/index.html",
        "safe"
    );

    write_file(
        outside / "secret.html",
        "secret"
    );

    view::Engine engine{
        root
    };

    assert(
        engine.render(
            "safe/index"
        ) ==
        "safe"
    );

    assert(
        rejects_path(
            [&] {
                static_cast<void>(
                    engine.render(
                        "../outside/secret"
                    )
                );
            }
        )
    );

    assert(
        rejects_path(
            [&] {
                static_cast<void>(
                    engine.render(
                        (
                            outside /
                            "secret.html"
                        ).string()
                    )
                );
            }
        )
    );

    std::string embedded_nul{
        "safe/index"
    };

    embedded_nul.push_back('\0');
    embedded_nul += "ignored";

    assert(
        rejects_path(
            [&] {
                static_cast<void>(
                    engine.render(
                        embedded_nul
                    )
                );
            }
        )
    );

    std::error_code error;

    std::filesystem::
        create_directory_symlink(
            outside,
            root / "linked",
            error
        );

    if (!error) {
        assert(
            rejects_path(
                [&] {
                    static_cast<void>(
                        engine.render(
                            "linked/secret"
                        )
                    );
                }
            )
        );
    }

    error.clear();

    std::filesystem::create_symlink(
        outside / "secret.html",
        root / "secret-link.html",
        error
    );

    if (!error) {
        assert(
            rejects_path(
                [&] {
                    static_cast<void>(
                        engine.render(
                            "secret-link.html"
                        )
                    );
                }
            )
        );
    }

    error.clear();

    const auto root_link =
        base / "views-link";

    std::filesystem::
        create_directory_symlink(
            root,
            root_link,
            error
        );

    if (!error) {
        assert(
            rejects_path(
                [&] {
                    view::Engine linked_root{
                        root_link
                    };

                    static_cast<void>(
                        linked_root
                    );
                }
            )
        );
    }

    bool missing = false;

    try {
        static_cast<void>(
            engine.render(
                "safe/missing"
            )
        );
    } catch (
        const view::NotFound&
    ) {
        missing = true;
    }

    assert(missing);

    std::filesystem::remove_all(
        base
    );

    return 0;
}
