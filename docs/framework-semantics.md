# Framework Semantic Contracts

> This guide describes the 1.0 contract and its documented limits. See [release scope](release-v1.md).

## Why this exists

A declaration keyword is more than a class-shaped syntax shortcut. Each framework artifact has runtime obligations. Before Phase 13, several invalid declarations could pass semantic validation and fail later in generated C++ or silently omit runtime registration.

Phase 13 moves those obligations into `ProgramValidator`.

## Contracts

| Artifact | Structured semantic contract |
| --- | --- |
| `model` | Metadata is constant and supported; lifecycle metadata synthesizes the ORM fields it requires; primary-key and incrementing combinations are validated. |
| `controller` | Implicit action results are `Response`; explicit helper methods remain typed normally. |
| `middleware` | Requires public `handle(Request, Next)` with logical result `Response`. Both sync and async handlers are supported. |
| `migration` | Requires public synchronous `void up()` and `void down()`. |
| `policy` | Every public ability is synchronous, returns `Decision`, and accepts exactly two non-optional model parameters: actor and resource. Bool expressions remain valid inside implicit `Decision` methods. |
| `event` | Contains immutable data fields only; methods, metadata and injected services are rejected. |
| `listener` | Requires public `void handle(Event)`; async handlers are supported and the event parameter must be a non-optional structured event. |
| `notification` | Requires public synchronous `via(Recipient) -> List<string>`; recipient is a non-optional model. `toMail` returns a structured mail declaration and `toDatabase` returns `Json`. |
| `mail` | Requires synchronous parameterless `subject() -> string`. `text()` and `html()` return string; `content()` returns `Response`. `html()` and `content()` cannot both define the HTML body. |
| `job` | Requires public `void handle()`; async handlers are supported. |

Private/protected helper methods remain available where they do not define a framework registration surface.

## Model lifecycle metadata

The structured model surface now makes lifecycle metadata self-consistent.

`timestamps = true` synthesizes:

```gnr
string? created_at;
string? updated_at;
```

`softDeletes = true` synthesizes:

```gnr
string? deleted_at;
```

These attributes are included in generated model metadata, so ORM save, touch, remove, restore, hydration and dirty tracking operate on the same declared model shape.

If an application explicitly declares a lifecycle field, its type must satisfy the same runtime contract. For example, `deleted_at` must remain nullable string data because restore assigns SQL null.

Metadata-only `casts` also infer native structured field types for bool, integer, double, decimal and JSON values.

## Diagnostics

Phase 13 reserves focused framework semantic diagnostics:

- `GNR2301` middleware contract;
- `GNR2302` migration contract;
- `GNR2303` listener/job contract;
- `GNR2304` policy contract;
- `GNR2305` event contract;
- `GNR2306` notification contract;
- `GNR2307` mail contract;
- `GNR2308` model metadata/lifecycle contract.

These diagnostics are emitted by the authoritative `gungnirc --check` path and therefore occur before C++ IR or native compilation.

## Examples

```gnr
middleware Authenticate {
    async handle(Request request, Next next) {
        return await next(request);
    }
}

policy UserPolicy {
    view(User actor, User resource) {
        return actor.id == resource.id;
    }
}

model Post {
    table = 'posts';
    timestamps = true;
    softDeletes = true;
}
```

## Boundaries

Phase 13 completes the semantic contract for the currently supported framework declaration surface. It does not add general classes, interfaces, polymorphic ORM relationships, arbitrary native overload resolution or automatic external service registration.

Runtime capabilities still matter after semantic validation. For example, notification channels must exist in the application, and database behavior still depends on the selected backend.

## Implementation references

- [src/language/validated.cpp](../src/language/validated.cpp)
- [src/language/cpp_ir.cpp](../src/language/cpp_ir.cpp)
- [tests/framework_semantics.cpp](../tests/framework_semantics.cpp)
- [tests/fixtures/structured/program.gnr](../tests/fixtures/structured/program.gnr)
- [tests/structured_generated.cpp](../tests/structured_generated.cpp)

See [Compiler Profiles](compiler-profiles.md), [Semantic Analysis](semantics.md), [Models](model.md), and [Testing](testing.md).
