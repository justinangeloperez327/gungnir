#include <cassert>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <gungnir/core/executor.hpp>
#include <gungnir/http/response.hpp>
#include <gungnir/view/engine.hpp>
#include <gungnir/view/runtime.hpp>

namespace {

void write_view(
    const std::filesystem::path& root,
    std::string_view value
) {
    std::filesystem::create_directories(
        root
    );

    std::ofstream output{
        root / "identity.html",
        std::ios::binary |
            std::ios::trunc
    };

    assert(output);

    output.write(
        value.data(),
        static_cast<std::streamsize>(
            value.size()
        )
    );

    assert(output);
}

std::string render_identity() {
    return std::string{
        gungnir::http::Response::view(
            "identity"
        ).body()
    };
}

} // namespace

int main() {
    using namespace gungnir;

    const auto base =
        std::filesystem::
            temp_directory_path() /
        "gungnir-view-runtime-test";

    const auto root_a =
        base / "a";

    const auto root_b =
        base / "b";

    std::filesystem::remove_all(
        base
    );

    write_view(
        root_a,
        "engine-a"
    );

    write_view(
        root_b,
        "engine-b"
    );

    auto engine_a =
        std::make_shared<
            view::Engine
        >(root_a);

    auto engine_b =
        std::make_shared<
            view::Engine
        >(root_b);

    view::runtime::use(
        engine_a
    );

    assert(
        render_identity() ==
        "engine-a"
    );

    std::vector<std::thread>
        renderers;

    std::mutex result_mutex;
    bool thread_failed = false;

    for (
        int index = 0;
        index < 8;
        ++index
    ) {
        auto engine =
            index % 2 == 0
                ? engine_a
                : engine_b;

        const auto expected =
            index % 2 == 0
                ? std::string{"engine-a"}
                : std::string{"engine-b"};

        renderers.emplace_back(
            [
                engine,
                expected,
                &result_mutex,
                &thread_failed
            ] {
                auto scope =
                    view::runtime::activate(
                        engine
                    );

                for (
                    int attempt = 0;
                    attempt < 100;
                    ++attempt
                ) {
                    if (
                        render_identity() !=
                        expected
                    ) {
                        std::lock_guard lock{
                            result_mutex
                        };

                        thread_failed = true;
                        return;
                    }
                }
            }
        );
    }

    for (auto& renderer : renderers) {
        renderer.join();
    }

    assert(!thread_failed);

    Executor executor{2};
    executor.start();

    std::mutex executor_mutex;
    std::condition_variable
        executor_ready;

    bool executor_done = false;
    std::string executor_result;

    {
        auto scope =
            view::runtime::activate(
                engine_b
            );

        executor.post(
            [&] {
                const auto result =
                    render_identity();

                {
                    std::lock_guard lock{
                        executor_mutex
                    };

                    executor_result =
                        result;

                    executor_done = true;
                }

                executor_ready
                    .notify_all();
            }
        );
    }

    {
        std::unique_lock lock{
            executor_mutex
        };

        executor_ready.wait(
            lock,
            [&] {
                return executor_done;
            }
        );
    }

    executor.stop();
    executor.join();

    assert(
        executor_result ==
        "engine-b"
    );

    assert(
        render_identity() ==
        "engine-a"
    );

    view::runtime::clear();

    bool missing_engine = false;

    try {
        static_cast<void>(
            render_identity()
        );
    } catch (
        const std::logic_error&
    ) {
        missing_engine = true;
    }

    assert(missing_engine);

    std::filesystem::remove_all(
        base
    );

    return 0;
}
