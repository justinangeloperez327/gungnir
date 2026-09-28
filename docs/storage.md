# Filesystem and Storage

Gungnir storage uses named disks behind the `storage::Disk` contract. `LocalDisk` provides filesystem-backed storage rooted at one configured directory.

## Local disk

`LocalDisk` supports reading, writing, existence checks, deletion, copying, moving, size inspection and immediate-directory file listing.

### Path containment

Local storage accepts relative object paths only. Absolute paths, embedded null bytes and lexical `..` traversal are rejected.

The storage root is pinned by filesystem identity when `LocalDisk` is constructed. If the configured root pathname is later replaced with a different directory, operations reject it rather than silently following the replacement.

On POSIX platforms, object operations reopen and verify the pinned root, walk parent directories with descriptor-relative `openat(..., O_NOFOLLOW)`, create directories with `mkdirat`, and perform final reads/writes/removes/moves relative to verified directory descriptors. Atomic commits use `renameat` inside the pinned destination directory. Directory listing uses the already-open directory descriptor. A concurrent symlink swap can therefore make an operation fail, but cannot redirect object I/O outside the pinned storage root.

On Windows, Gungnir pins the root by volume/file identity, opens objects and directories with `FILE_FLAG_OPEN_REPARSE_POINT`, rejects reparse points, verifies resolved handle paths remain under the pinned root, and uses handle-based delete/rename operations with a verified destination-directory handle. Temporary candidates are verified before application data is written. A concurrently hostile process may still cause a candidate create to fail or create-and-delete an empty candidate before verification; Gungnir does not claim the same descriptor-relative create primitive that POSIX provides.

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
