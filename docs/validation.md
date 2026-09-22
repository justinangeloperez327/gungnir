# Validation

Gungnir validation separates rule declaration, validation results and HTTP exception behavior.

`Rules` supports initializer declarations and programmatic composition with `add(field, expression)`. Existing rules include required, present, nullable, sometimes, string, integer, numeric, boolean, email, accepted, length, min, max, in, same and confirmed.

`Validator::check` returns a `Result` containing validated values and field errors without throwing. `Validator::validate` is the convenience path that throws `ValidationException` when validation fails.

`Request::check` and `Request::validate` expose both flows directly on HTTP input.

`ValidatedRequest` is the foundation for dedicated request validation classes. A request type defines its rules and exposes `validated()`, allowing controllers to depend on a validation-oriented abstraction rather than repeating rule sets inside actions.

Gungnir source keeps the concise object syntax:

```gungnir
data = request.validate({
    "name": "required|min:2|max:100",
    "email": "required|email"
});
```

The transpiler lowers this into native `validation::Rules`; application developers do not need to write the C++ rule container manually.

Database-aware rules such as unique and exists belong with Group 12 database integration. They should use parameterized database queries and must not make the core validator depend directly on a specific database driver.
