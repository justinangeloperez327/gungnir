# Filesystem and Storage

Gungnir storage uses named disks behind the `storage::Disk` contract. `LocalDisk` provides filesystem-backed storage rooted at one configured directory.

## Local disk

`LocalDisk` supports reading, writing, existence checks, deletion, copying, moving, size inspection and immediate-directory file listing.

### Path containment

Local storage accepts relative object paths only. Absolute paths, embedded null bytes and lexical `..` traversal are rejected.

Before resolving an object, `LocalDisk` walks existing path components and rejects symbolic links. The candidate is then canonicalized and verified to remain under the canonical storage root. Destination paths are checked again after parent-directory creation.

This closes ordinary symlink-escape paths such as a child under the storage root that points outside the configured root.

These checks do not claim race-free containment against a hostile process that can concurrently replace directory components between validation and an operating-system call. Strong adversarial multi-process containment would require descriptor-relative platform primitives such as `openat2` or equivalent directory-handle APIs.

### Atomic writes

`LocalDisk::put()` does not truncate the destination in place. It creates an exclusive temporary file in the destination directory, writes and flushes the new content, checks cancellation, and then atomically replaces the destination using the platform replacement primitive.

If writing fails or cancellation is observed before replacement, the temporary object is removed and the previous destination remains unchanged.

Atomic replacement prevents readers from observing a partially written object. It does not by itself guarantee full power-loss durability of directory metadata on every filesystem.

## Cancellation

Every `Disk` operation has a cancellation-aware overload. Generic disks receive before/after cancellation checkpoints by default.

`LocalDisk` additionally checks cancellation while reading and writing buffered file content. A cancelled `put()` never commits its temporary file once cancellation has been observed.

```cpp
disk.put(
    "reports/monthly.txt",
    contents,
    request.cancellation()
);
```

Cancellation remains cooperative. A filesystem system call already executing in the kernel is not forcibly terminated.

## Named disks

`storage::Manager` maps application names such as `local` or `uploads` to concrete disk implementations and provides a configurable default.

## Remote storage

S3-compatible, Azure Blob and other remote systems should be implemented as real `Disk` adapters backed by their official or reviewed clients. Gungnir does not pretend that a local filesystem adapter provides cloud semantics.

## Uploads

HTTP upload parsing and storage are separate concerns. Uploaded filenames must never be used directly as trusted filesystem paths. Applications should generate storage keys and retain the original filename only as metadata.

## Security

Public-file serving requires a separate HTTP policy for content type, cache headers, authorization and download disposition.
