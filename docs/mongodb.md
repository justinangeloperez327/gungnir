# MongoDB Adapter

Gungnir provides an optional MongoDB document adapter backed by the official MongoDB C Driver (`libmongoc`).

Enable it when configuring Gungnir:

```sh
cmake -S . -B build -DGUNGNIR_WITH_MONGODB=ON
```

The exported CMake target is:

```cmake
target_link_libraries(app PRIVATE gungnir::mongodb)
```

## Registration

Register the adapter before application database configuration:

```cpp
gungnir::database::register_mongodb(
    app.database_drivers()
);
```

Use `DB_CONNECTION=mongodb`.

## Document commands

MongoDB remains document-native. Gungnir's MongoDB ORM compiler emits JSON command documents with separate bindings. The adapter resolves `{"$bind":N}` placeholders into typed BSON values before execution.

Queries use MongoDB find and aggregate cursors rather than translating document operations into SQL.

## Model primary keys

Gungnir model field `id` maps to MongoDB `_id`. Query results map `_id` back to `id` for model hydration.

For ordinary incrementing integer models, the adapter allocates an integer `_id` through an atomic `gungnir_sequences` collection. This preserves the normal `PrimaryKey<Integer>` model surface while retaining MongoDB's native `_id` primary-key index.

An explicitly supplied model `id` is stored directly as `_id`.

## Schema and migrations

Migration plans compile to MongoDB commands such as `create`, `collMod`, `createIndexes`, `dropIndexes`, `update`, and `drop`. MongoDB does not enforce relational foreign keys.

Gungnir migration timestamps are serialized as strings, so MongoDB validators use BSON string validation for date/time migration fields.

Standalone MongoDB connections currently report transactions as unsupported. The migration runner respects driver transaction capability and executes MongoDB migration steps without pretending that a standalone deployment provides multi-document transactions.

## Value mapping

BSON null, boolean, signed integer, double, UTF-8 string, ObjectId, date, Decimal128, document and array values are converted into Gungnir model values. Nested documents and arrays are represented as canonical Extended JSON strings because the current `AttributeValue` type is scalar.

## Connection options

`DB_OPTIONS` is appended to the MongoDB URI query string, allowing driver options such as replica-set, retry, timeout, and TLS settings without hard-coding them into Gungnir core.
