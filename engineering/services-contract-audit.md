# Canonical cache and storage contract audit

Baseline: `d467cd8e5cd33ae38887e2cd3f8b2f0e46ede268`, the verified head of
authentication PR #178. That head integrates main
`261a262c4bc964f0b8c916287d6583fdb6a2fd4c` and preserves the combined ORM,
HTTP and request-context contracts. This batch depends on those repairs.
PR #178 has since merged as main commit
`13a53045776ed919ae791bb6f84e52dd559a6452` with the same verified tree; this
branch integrates that main commit.

## Proven contract gap

The cache and storage product guides describe service calls, but canonical
validation rejected injected `Cache` and `Storage` types. Native cache
repositories, memory/Redis stores, local/S3 disks, safe path handling and service
registration already existed. These adapters and their direct native APIs are
preserved.

## Typed services and ownership

`Cache` uses an owned JSON-value facade over the existing string repository.
`get` returns `Json?`; missing entries and stored JSON null remain distinct.
`put`, `has`, `forget`, `flush` and synchronous zero-parameter `remember`
factories use the configured adapter. Whole-second lifetimes reject negative or
unrepresentable expiration before writes; zero expires immediately. Concurrent
misses retain the native repository's factory behavior.

`Storage` retains the existing manager and delegates default-disk operations.
`disk` returns an owned `StorageDisk`; a selected disk survives manager
replacement and application destruction. Reads return `string?`, size is an
unsigned byte count, and logical file paths are owned strings. Native
`Manager::disk` remains available alongside the new shared-owner lookup.

`ServicesProvider` registers both canonical facades alongside native services.
Injected fields, function arguments, captured factories and awaited middleware
retain their service owners. Handles cannot be serialized or stored as model
fields. Missing registrations and unknown disks raise configuration errors.

Calls pass through canonical semantic validation and structural C++ IR.
Diagnostics cover arity, keys, lifetimes, factory signatures, optional reads,
static calls and opaque values. Request/response objects and service/callback
handles are rejected before emission, including inside objects and mixed lists.
Explicit native extensions remain responsible for their JSON codec.

## Shared serialization and lowering repairs

JSON serialization preserves optional values and visible model attributes.
The optional codec definition now sees model, map and range overloads, allowing
nested optional containers and optional models to compile. Parsing retains the
full unsigned 64-bit integer range. Floating-point output preserves its JSON
number representation, including negative zero, and uses the classic locale;
non-finite values are rejected before storage.

Structural IR decays named function arguments to callable values while retaining
left-to-right argument evaluation. Optional string results are returned as owned
optionals rather than incorrectly converted to plain strings. Existing receiver
ownership, schema-reference and asynchronous result rules are retained.

## Windows disk rename repair

The generated service fixture exposed Windows error 87 in the local disk's
directory-relative `SetFileInformationByHandle` rename. The disk now calls
`NtSetInformationFile` with `FileRenameInformation` through the loaded system
module. It retains the verified destination-directory handle, replacement
semantics and existing path/reparse checks. Failure codes are converted to
Windows errors; no destination-path retry is introduced.

The Windows SDK declares the native call's types in `winternl.h`, but exposes
only a subset of the information-class enum. Class 10 and the relative-name
layout are defined by Microsoft's
[native file information API](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-ntsetinformationfile)
and
[rename structure](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/ns-ntifs-_file_rename_information).
The generated fixture writes the same binary file twice directly before
exercising routed default/named disk operations, making replacement failures
visible before HTTP exception sanitization. Its request helper also preserves
the method and body before transferring body ownership.

## Acceptance evidence

`tests/fixtures/structured/services.gnr` and `tests/structured_services.cpp`
execute generated controllers and awaited middleware through an application
configured with memory cache and named local disks. They cover misses/hits,
named and captured factories, nested calls, failed factories, TTL handling,
cached null, numeric fidelity, optional scalars/models/containers, model
visibility, malformed native cache bytes, binary file contents, default/named
disk isolation, file operations, path rejection and retained service ownership.
Invalid programs have matching emission and validation-only diagnostics/spans.

Local GCC 13 Debug, SQLite and password hashing enabled, TLS disabled:
**94/94 CTest checks pass** on the final code, including installed CLI generation,
migrations, live HTTP and development restart. Damaged local object/executable
artifacts were rebuilt before execution; no repository workarounds were added.
**Seven installed SDK consumers pass**: native public API, ORM contract, SQLite
ORM, HTTP, request context, authentication and cache/storage services.
**All six public cache/storage language examples pass strict validation**.
Documentation/development consistency, workflow YAML and whitespace checks pass.

CI runs generated services and installed-package execution. Compiler conformance
compares service C++, validated AST and IR across GCC, Clang and MSVC alongside
all existing fixtures. Exact published-head remote results are recorded in the
pull request after verification.

## Remaining product requirements

Distributed cache locks, live Redis equivalence, request upload integration,
storage streaming/additional metadata, live S3-compatible verification and the
full application acceptance gate still need independent evidence. Existing
native cancellation and adapter behavior is preserved, but this fixture does not
establish those additional language contracts or live backend/platform matrices.
The baseline product matrix remains the record of wider requirements.
