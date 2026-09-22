# Filesystem and Storage

Gungnir storage uses named disks behind the `storage::Disk` contract.

## Local disk

`LocalDisk` confines relative object paths beneath a configured root. Absolute paths and parent traversal are rejected.

It supports reading, writing, existence checks, deletion, copying, moving, size inspection and immediate-directory file listing.

## Named disks

`storage::Manager` maps application names such as `local` or `uploads` to concrete disk implementations and provides a configurable default.

## Remote storage

S3-compatible, Azure Blob and other remote systems should be implemented as real `Disk` adapters backed by their official or reviewed clients. Gungnir does not pretend that a local filesystem adapter provides cloud semantics.

## Uploads

HTTP upload parsing and storage are separate concerns. Uploaded filenames must never be used directly as trusted filesystem paths. Applications should generate storage keys and retain the original filename only as metadata.

## Security

Local storage rejects absolute paths and `..` traversal. Public-file serving requires a separate HTTP policy for content type, cache headers, authorization and download disposition.
