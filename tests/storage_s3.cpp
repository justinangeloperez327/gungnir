#include <gungnir/storage/s3_disk.hpp>
#include <cassert>
#include <stdexcept>

int main(int argc, char** argv) {
    assert(argc == 2);
    using namespace gungnir;
    storage::S3Options options{.endpoint=argv[1], .bucket="test", .access_key="test-key",
        .secret_key="test-secret", .session_token="test-session", .max_object_bytes=1024, .allow_http=true};
    storage::S3Disk disk{options};
    assert(!disk.exists("absent") && !disk.get("absent"));
    disk.put("folder/a b%.txt", std::string{"a\0b",3});
    assert(disk.exists("folder/a b%.txt") && disk.size("folder/a b%.txt")==3);
    assert(disk.get("folder/a b%.txt")==std::string("a\0b",3));
    assert(disk.copy("folder/a b%.txt","folder/copy"));
    assert(disk.move("folder/copy","folder/moved") && !disk.exists("folder/copy"));
    assert(disk.files("folder")==std::vector<std::string>({"folder/a b%.txt","folder/moved"}));
    assert(disk.remove("folder/moved") && !disk.exists("folder/moved"));
    bool rejected=false;
    try { disk.get("../escape"); } catch (const std::invalid_argument&) { rejected=true; }
    assert(rejected);
    rejected=false;
    try { disk.get("error"); } catch (const std::runtime_error&) { rejected=true; }
    assert(rejected);
    rejected=false;
    try { disk.get("large"); } catch (const std::length_error&) { rejected=true; }
    assert(rejected);
    rejected=false;
    try { disk.files("repeat"); } catch (const std::runtime_error&) { rejected=true; }
    assert(rejected);
    disk.put("folder/second", "two");
    options.max_list_entries=1;
    storage::S3Disk bounded{options};
    rejected=false;
    try { bounded.files("folder"); } catch (const std::length_error&) { rejected=true; }
    assert(rejected);
    CancellationSource source; source.cancel();
    rejected=false;
    try { disk.get("absent",source.token()); } catch (const OperationCancelled&) { rejected=true; }
    assert(rejected);
}
