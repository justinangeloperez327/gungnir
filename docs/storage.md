# Storage

Gungnir provides a filesystem abstraction for application files and object storage.

## Disks

Applications configure named storage disks and select a default disk.

Common deployments use:

- local filesystem storage;
- S3-compatible object storage.

## Writing files

```gnr
storage.put("reports/monthly.csv", contents);
```

## Reading files

```gnr
const contents = storage.get("reports/monthly.csv");
```

## File operations

Storage supports existence checks, deletion, copying/moving, metadata, and stream-oriented operations for large files.

## Paths

Logical storage paths are resolved beneath the configured disk root or object-store prefix. Applications should not concatenate untrusted filesystem paths directly.

## Uploads

Validated request uploads can be written to a configured storage disk.

## S3-compatible storage

Object-storage disks use the configured endpoint, region, bucket, credentials, and prefix. S3-compatible services can be used when they implement the required API behavior.
