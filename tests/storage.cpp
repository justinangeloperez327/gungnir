#include <cassert>
#include <filesystem>
#include <fstream>
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

    std::filesystem::remove_all(base);

    return 0;
}
