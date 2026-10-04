# Storage

Inject `Storage` to access the configured default disk. Named disks are selected with `storage.disk(name)`, which returns an owned `StorageDisk` handle.

## Configuration

Configure named local or S3-compatible disks in the native application bootstrap. Register the manager before resolving generated controllers:

```cpp
auto disks = std::make_shared<storage::Manager>();
disks->add("local", std::make_shared<storage::LocalDisk>("storage/app"));
disks->default_disk("local");
Application app;
app.provider<ServicesProvider>(ServiceOptions{.storage = disks});
app.boot();
```

The service retains the manager; a selected disk handle retains that disk even when its named configuration is replaced.

## Writing and reading files

```gnr
controller FileController {
    inject Storage storage;

    store(Request request) {
        storage.put("reports/monthly.csv", request.body());
        return noContent();
    }

    show(Request request) {
        const contents = storage.get("reports/monthly.csv");
        if (contents != null) { return text(contents); }
        return response("Missing file", 404);
    }
}
```

`put` accepts string contents, including binary data. `get` returns `string?`; missing files are absent, and an empty file remains a present value.

## Named disks

```gnr
controller ArchiveController {
    inject Storage storage;

    store(Request request) {
        const archive = storage.disk("archive");
        archive.put("monthly.csv", request.body());
        return json(archive.exists("monthly.csv"));
    }
}
```

Calling `disk()` without a name selects the default. Selecting an unconfigured disk raises a configuration error. Disk handles can be passed through functions and awaited work; application data is selected explicitly when serializing a response.

## File operations

The same operations are available on the default service and a selected disk:

```gnr
function Json manageReports(Storage storage) {
    const copied = storage.copy("reports/monthly.csv", "reports/copy.csv");
    const moved = storage.move("reports/copy.csv", "reports/archive.csv");
    const bytes = storage.size("reports/archive.csv");
    const removed = storage.remove("reports/archive.csv");
    return {"copied": copied, "moved": moved, "bytes": bytes,
        "removed": removed, "files": storage.files("reports")};
}
```

`exists`, `remove`, `copy` and `move` return booleans. `size` returns an unsigned byte count, and `files` returns owned logical paths. Storage also supports metadata and stream-oriented operations for large files.

## Paths

Logical paths resolve beneath the configured disk root or object-store prefix. Local disks reject absolute paths, traversal and symlink escapes. Applications should not concatenate untrusted filesystem paths directly.

## Uploads

Validated request uploads can be written to a configured storage disk.

## S3-compatible storage

Object-storage disks use the configured endpoint, region, bucket, credentials and prefix. S3-compatible services can be used when they implement the required API behavior.
