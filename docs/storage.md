# Filesystem and Storage

Gungnir storage uses named disks behind the `storage::Disk` contract. `LocalDisk` provides filesystem-backed storage rooted at one configured directory.

## Local disk

`LocalDisk` supports reading, writing, existence checks, deletion, copying, moving, size inspection and immediate-directory file listing.

### Path containment

Local storage accepts relative object paths only. Absolute paths, embedded null bytes and lexical `..` traversal are rejected.

The storage root is pinned by filesystem identity when `LocalDisk` is constructed. If the configured root pathname is later replaced with a different directory, operations reject it rather than silently following the replacement.

On POSIX platforms, object operations reopen and verify the pinned root, walk parent directories with descriptor-relative `openat(..., O_NOFOLLOW)`, create directories with `mkdirat`, and perform final reads/writes/removes/moves relative to verified directory descriptors. Atomic commits use `renameat` inside the pinned destination directory. Directory listing uses the already-open directory descriptor. A concurrent symlink swap can therefore make an operation fail, but cannot redirect object I/O outside the pinned storage root.

On Windows, Gungnir pins the root by volume/file identity, opens objects and directories with `FILE_FLAG_OPEN_REPARSE_POINT`, rejects reparse points, verifies resolved handle paths remain under the pinned root, and uses handle-based delete/rename operations with a verified destination-directory handle. Temporary candidates are verified before application data is written. A concurrently hostile process may still cause a candidate create to fail or create-and-delete an empty candidate before verification; Gungnir does not claim the same descriptor-relative create primitive that POSIX provides.

### Atomic and durable writes

`LocalDisk::put()` does not truncate the destination in place. It creates an exclusive temporary file in the destination directory, writes the complete new content, flushes the file, checks cancellation, and then atomically replaces the destination.

On POSIX, Gungnir `fsync()`s the file before `renameat()` and then `fsync()`s the containing directory. Newly created parent-directory levels are also followed by a parent-directory metadata sync. Removes and moves sync the affected directory metadata after the mutation. This provides the normal crash-durability contract expected from local filesystems that honor file and directory `fsync()`.

On Windows, file contents are flushed with `FlushFileBuffers()` before the handle-based rename. Gungnir also attempts to flush verified directory handles after metadata mutations. Windows filesystems do not uniformly permit directory `FlushFileBuffers()`; `ERROR_INVALID_HANDLE` and `ERROR_ACCESS_DENIED` are treated as an unsupported directory-flush capability rather than pretending to provide a POSIX-equivalent guarantee.

If writing fails or cancellation is observed before replacement, the temporary object is removed and the previous destination remains unchanged.

### Abandoned temporary files

`LocalDisk::cleanup_abandoned(older_than)` explicitly removes Gungnir temporary artifacts older than the supplied age. The default threshold is 24 hours. Cleanup recognizes only Gungnir's hidden `.gungnir-...tmp` naming pattern; unrelated files and fresh temporary files are left untouched.

Cleanup is explicit rather than unconditional at startup so one process does not silently delete another long-running process's temporary write. Candidates are passed back through the disk's hardened remove path before deletion.

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
