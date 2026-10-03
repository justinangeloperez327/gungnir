# MongoDB Adapter

> **Gungnir 1.x design specification.** This document describes intended long-term architecture beyond the frozen `1.0.0-rc.1` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../mongodb.md) before using an API.


## 1.0 RC alignment

The shipped baseline is `1.0.0-rc.1`. The 1.0 contracts are frozen; this design document may describe additive 1.x evolution or future-major work, but current implementation guides and executable tests remain authoritative.

Gungnir provides an optional MongoDB adapter backed by the official MongoDB C Driver.

MongoDB is document-native.

Gungnir may provide a familiar model/ORM surface where semantics can be mapped safely, but the adapter must not pretend MongoDB provides relational guarantees that it does not have.

# Registration

The adapter registers explicitly with the database driver registry during application bootstrap.

A configured MongoDB connection may then be selected by the ORM and migration/runtime layers.

# Native document execution

MongoDB operations should compile to document/BSON operations rather than SQL.

Parameter/binding concepts should remain explicit so runtime values are not spliced into command text unsafely.

# Queries

The adapter may use find, aggregate, update, insert, delete, and other MongoDB-native operations.

ORM operations are translated according to their validated semantic identity.

Unsupported relational-only operations should produce clear capability errors.

# Model identity

MongoDB uses _id as the native primary key.

Gungnir may expose an application model id abstraction and map it to _id.

The exact strategy depends on model key configuration.

If the framework provides generated integer IDs through a sequence collection, that behavior is a Gungnir compatibility layer rather than a native MongoDB auto-increment feature.

# Relationships

MongoDB does not enforce relational foreign keys.

Gungnir relationship conveniences may represent application query conventions, but they do not create database-enforced referential integrity unless explicitly implemented through application logic.

# Migrations

MongoDB migration operations may map to commands such as:

~~~text
create
collMod
createIndexes
dropIndexes
update
drop
~~~

Schema validation is different from relational DDL.

The migration compiler/runtime should expose backend capability differences clearly.

# Transactions

Transaction support depends on MongoDB deployment topology and driver/runtime support.

Standalone deployments do not provide the same transaction capabilities as properly configured replica sets or sharded deployments.

The adapter must report capabilities rather than assuming transaction availability.

# Value mapping

BSON values map into Gungnir runtime values according to explicit rules.

Important categories include:

~~~text
null
bool
integer
double
decimal where supported
string
date/time
object/document
array
object ID
binary
~~~

The target mapping should preserve Gungnir decimal semantics when BSON Decimal128 is supported.

# Object IDs

Applications using native ObjectId keys require an explicit Gungnir/runtime key representation.

Do not silently coerce ObjectId into an unrelated integer type.

# Connection configuration

Configuration may include:

~~~text
URI / hosts
database
authentication
TLS
replica set
timeouts
pool options
application name
~~~

Credentials belong in secrets/configuration.

# Cancellation

Operations should observe runtime cancellation where supported by the driver path.

If cancellation is only cooperative at operation boundaries, that limitation should remain explicit.

# Async behavior

A blocking MongoDB driver call is not made non-blocking by use inside an async action.

The runtime must use genuine non-blocking support or an explicit blocking/offload executor.

# Health

A bounded ping/command may provide live readiness information.

# Security

Production deployments should use appropriate authentication, authorization, TLS, and network restrictions.

MongoDB command documents and logs must not expose credentials.

# Integration testing

Live tests should cover:

- CRUD;
- model key mapping;
- query translation;
- indexes/migrations;
- transaction capability behavior;
- BSON value mapping;
- cancellation where supported.

# Design rule

~~~text
MongoDB remains document-native
Gungnir exposes common application semantics only where valid
capability differences stay explicit
no fake relational guarantees
~~~

