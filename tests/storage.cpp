#include <cassert>
#include <filesystem>
#include <memory>
#include <string>

#include <gungnir/storage/storage.hpp>

int main() {
    using namespace gungnir;
    const auto root = std::filesystem::temp_directory_path() / "gungnir-storage-test";
    std::filesystem::remove_all(root);

    auto local = std::make_shared<storage::LocalDisk>(root);
    storage::Manager storage;
    storage.add("local", local).default_disk("local");

    auto& disk = storage.disk();
    disk.put("documents/a.txt", "Gungnir");
    assert(disk.exists("documents/a.txt"));
    assert(disk.get("documents/a.txt").value() == "Gungnir");
    assert(disk.size("documents/a.txt") == 7);
    assert(disk.copy("documents/a.txt", "documents/b.txt"));
    assert(disk.move("documents/b.txt", "documents/c.txt"));
    assert(disk.exists("documents/c.txt"));
    assert(disk.remove("documents/c.txt"));

    bool rejected = false;
    try {
        disk.put("../escape.txt", "no");
    } catch (const storage::InvalidPath&) {
        rejected = true;
    }
    assert(rejected);

    std::filesystem::remove_all(root);
    return 0;
}
