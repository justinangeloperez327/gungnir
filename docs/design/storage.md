# Filesystem and Storage

> **Gungnir 1.x design specification.** This document describes intended long-term architecture beyond the frozen `1.0.0-rc.1` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../storage.md) before using an API.


## 1.0 RC alignment

The shipped baseline is `1.0.0-rc.1`. The 1.0 contracts are frozen; this design document may describe additive 1.x evolution or future-major work, but current implementation guides and executable tests remain authoritative.

Gungnir storage exposes named disks behind a storage contract.

Application code addresses logical object paths. Adapters implement local filesystem or remote/object storage behavior.

# Disk operations

A disk may support:

~~~text
read
write
exists
delete
copy
move
size
list
metadata
streaming
~~~

Capabilities should be explicit per adapter.

# Named disks

Applications configure named disks such as:

~~~text
local
public
uploads
archive
remote
~~~

The logical disk name is application configuration, not a raw filesystem path.

# Local disk

LocalDisk is rooted at one configured directory.

Application object paths must remain relative to that root.

Reject:

- absolute paths;
- NUL characters;
- parent traversal;
- unsafe symbolic-link/reparse traversal.

# Path containment

Containment must remain safe under concurrent filesystem changes as far as the platform implementation guarantees.

POSIX implementations should prefer descriptor-relative no-follow operations.

Windows implementations should use handle/reparse-point verification appropriate to the platform.

Adapter docs must state any remaining race limitations honestly.

# Writes

Where durable/atomic replacement is promised, the adapter must implement the required file and directory flush/rename semantics.

Do not label a simple overwrite as durable atomic storage without those guarantees.

# Temporary files

Atomic write implementations should clean up abandoned temporary files where practical.

Temporary candidates must remain inside the trusted storage root.

# Cancellation

Long storage operations may expose cancellation-aware APIs.

Cancellation should leave destination state according to a documented contract.

Partial/corrupt committed output should not be presented as a successful write.

# Remote storage

Remote/object storage adapters should preserve the logical disk contract while documenting differences such as:

~~~text
eventual consistency
multipart upload
metadata behavior
rename implemented as copy+delete
conditional writes
versioning
ETags
~~~

Do not pretend remote object stores have local filesystem semantics.

# Uploads

Uploaded files are untrusted request data.

The storage layer can persist bytes, but the application must decide authorization, validation, filename policy, retention, and content rules.

Browser-provided filenames are metadata, not safe paths.

# Public files

Public URL generation should be explicit per disk/configuration.

A stored object does not automatically become public.

# Security

Storage credentials belong in configuration/secrets.

Object paths should not expose server filesystem layout.

Sensitive objects require correct access-control policy.

# Async behavior

A storage operation is only truly async when the adapter/runtime avoids blocking the request executor.

Blocking filesystem or SDK calls should use the runtime's blocking/offload strategy when invoked from async request paths.

# Testing

Tests should use isolated temporary roots or test adapters with deterministic cleanup.

# Design rule

~~~text
application uses logical disks and object paths
adapter owns transport/filesystem semantics
containment and durability guarantees must be explicit
~~~

