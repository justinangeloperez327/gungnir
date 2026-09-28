#include <cassert>
#include <thread>
#include <chrono>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

#include <gungnir/core/cancellation.hpp>
#include <gungnir/storage/storage.hpp>

namespace {

void write_plain(
    const std::filesystem::path& path,
    std::string_view value
) {
    std::ofstream output{
        path,
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

} // namespace

int main() {
    using namespace gungnir;

    const auto base =
        std::filesystem::
            temp_directory_path() /
        "gungnir-storage-test";

    const auto root =
        base / "root";

    const auto outside =
        base / "outside";

    std::filesystem::remove_all(base);
    std::filesystem::create_directories(
        outside
    );

    write_plain(
        outside / "secret.txt",
        "outside"
    );

    auto local =
        std::make_shared<
            storage::LocalDisk
        >(root);

    storage::Manager storage;
    storage.add(
        "local",
        local
    ).default_disk(
        "local"
    );

    auto& disk =
        storage.disk();

    disk.put(
        "documents/a.txt",
        "Gungnir"
    );

    assert(
        disk.exists(
            "documents/a.txt"
        )
    );

    assert(
        disk.get(
            "documents/a.txt"
        ).value() ==
        "Gungnir"
    );

    assert(
        disk.size(
            "documents/a.txt"
        ) ==
        7
    );

    assert(
        disk.copy(
            "documents/a.txt",
            "documents/b.txt"
        )
    );

    assert(
        disk.move(
            "documents/b.txt",
            "documents/c.txt"
        )
    );

    assert(
        disk.exists(
            "documents/c.txt"
        )
    );

    assert(
        disk.remove(
            "documents/c.txt"
        )
    );

    bool traversal_rejected = false;

    try {
        disk.put(
            "../escape.txt",
            "no"
        );
    } catch (
        const storage::InvalidPath&
    ) {
        traversal_rejected = true;
    }

    assert(traversal_rejected);

    bool root_remove_rejected = false;

    try {
        disk.remove("");
    } catch (
        const storage::InvalidPath&
    ) {
        root_remove_rejected = true;
    }

    assert(root_remove_rejected);

    disk.put(
        "atomic.txt",
        "before"
    );

    CancellationSource cancelled;
    cancelled.cancel();

    bool cancelled_write = false;

    try {
        disk.put(
            "atomic.txt",
            std::string(
                1024 * 1024,
                'x'
            ),
            cancelled.token()
        );
    } catch (
        const OperationCancelled&
    ) {
        cancelled_write = true;
    }

    assert(cancelled_write);

    assert(
        disk.get(
            "atomic.txt"
        ).value() ==
        "before"
    );

    bool cancelled_read = false;

    try {
        static_cast<void>(
            disk.get(
                "atomic.txt",
                cancelled.token()
            )
        );
    } catch (
        const OperationCancelled&
    ) {
        cancelled_read = true;
    }

    assert(cancelled_read);

    for (
        const auto& path :
        disk.files()
    ) {
        assert(
            path.find(
                ".gungnir-"
            ) ==
            std::string::npos
        );
    }

    std::error_code symlink_error;

    std::filesystem::
        create_directory_symlink(
            outside,
            root / "linked",
            symlink_error
        );

    if (!symlink_error) {
        bool symlink_rejected = false;

        try {
            static_cast<void>(
                disk.get(
                    "linked/secret.txt"
                )
            );
        } catch (
            const storage::InvalidPath&
        ) {
            symlink_rejected = true;
        }

        assert(symlink_rejected);

        bool symlink_write_rejected =
            false;

        try {
            disk.put(
                "linked/new.txt",
                "escape"
            );
        } catch (
            const storage::InvalidPath&
        ) {
            symlink_write_rejected =
                true;
        }

        assert(
            symlink_write_rejected
        );

        assert(
            !std::filesystem::exists(
                outside / "new.txt"
            )
        );
    }

    symlink_error.clear();

    std::filesystem::create_symlink(
        outside / "secret.txt",
        root / "secret-link.txt",
        symlink_error
    );

    if (!symlink_error) {
        bool file_symlink_rejected =
            false;

        try {
            static_cast<void>(
                disk.get(
                    "secret-link.txt"
                )
            );
        } catch (
            const storage::InvalidPath&
        ) {
            file_symlink_rejected =
                true;
        }

        assert(
            file_symlink_rejected
        );
    }

    const auto identity_root =
        base / "identity-root";

    const auto replaced_root =
        base / "identity-old";

    storage::LocalDisk
        identity_disk{
            identity_root
        };

    identity_disk.put(
        "value.txt",
        "pinned"
    );

    std::filesystem::rename(
        identity_root,
        replaced_root
    );

    std::filesystem::
        create_directories(
            identity_root
        );

    write_plain(
        identity_root /
            "value.txt",
        "replacement"
    );

    bool root_replacement_rejected =
        false;

    try {
        static_cast<void>(
            identity_disk.get(
                "value.txt"
            )
        );
    } catch (
        const storage::InvalidPath&
    ) {
        root_replacement_rejected =
            true;
    }

    assert(
        root_replacement_rejected
    );

#ifndef _WIN32
    const auto race_root =
        base / "race-root";

    const auto race_outside =
        base / "race-outside";

    std::filesystem::
        create_directories(
            race_outside
        );

    write_plain(
        race_outside /
            "value.txt",
        "outside"
    );

    storage::LocalDisk
        race_disk{
            race_root
        };

    race_disk.put(
        "slot/value.txt",
        "inside"
    );

    const auto slot =
        race_root / "slot";

    const auto held =
        race_root / "slot-held";

    std::atomic_bool
        attack_done{false};

    std::thread attacker{
        [&] {
            for (
                int attempt = 0;
                attempt < 500;
                ++attempt
            ) {
                std::error_code error;

                std::filesystem::rename(
                    slot,
                    held,
                    error
                );

                if (error) {
                    std::this_thread::
                        yield();
                    continue;
                }

                error.clear();

                std::filesystem::
                    create_directory_symlink(
                        race_outside,
                        slot,
                        error
                    );

                if (!error) {
                    std::this_thread::
                        yield();

                    error.clear();

                    std::filesystem::remove(
                        slot,
                        error
                    );
                }

                error.clear();

                std::filesystem::rename(
                    held,
                    slot,
                    error
                );
            }

            attack_done.store(
                true,
                std::memory_order_release
            );
        }
    };

    bool observed_outside = false;

    for (
        int attempt = 0;
        attempt < 1000;
        ++attempt
    ) {
        try {
            const auto value =
                race_disk.get(
                    "slot/value.txt"
                );

            if (
                value &&
                *value == "outside"
            ) {
                observed_outside = true;
                break;
            }

            race_disk.put(
                "slot/value.txt",
                "inside"
            );
        } catch (
            const storage::InvalidPath&
        ) {
        } catch (
            const storage::Error&
        ) {
        }

        if (
            attack_done.load(
                std::memory_order_acquire
            )
        ) {
            break;
        }
    }

    attacker.join();

    assert(!observed_outside);

    std::ifstream outside_value{
        race_outside /
            "value.txt",
        std::ios::binary
    };

    assert(outside_value);

    std::string outside_contents{
        std::istreambuf_iterator<char>{
            outside_value
        },
        std::istreambuf_iterator<char>{}
    };

    assert(
        outside_contents ==
        "outside"
    );
#endif

    std::filesystem::remove_all(base);

    return 0;
}
