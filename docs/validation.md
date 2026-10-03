# Validation

Gungnir validates request and structured application data with declarative rules.

## Validating a request

```gnr
const data = request.validate({
    "name": "required|string|max:100",
    "email": "required|email",
    "age": "nullable|integer|min:18"
});
```

Successful validation returns validated data. Invalid input raises a validation exception that can be converted into an HTTP error response.

## Rules

Gungnir provides rules for presence, scalar types, strings, numbers, booleans, email addresses, accepted values, lengths, ranges, arrays, objects, UUIDs, URLs, dates, files, images, equality/confirmation, membership, conditional requirements, and database-backed uniqueness/existence checks.

Rules can be combined with pipe syntax.

## Optional values

Use `nullable` when null is accepted and `sometimes` when a field should only be validated when supplied.

## Nested data

Dot notation selects nested fields:

```gnr
const data = request.validate({
    "profile.name": "required|string",
    "profile.email": "required|email"
});
```

## Arrays and wildcards

```gnr
const data = request.validate({
    "users": "required|array",
    "users.*.email": "required|email"
});
```

## Database rules

```text
unique:users,email
exists:projects,id
```

Database validation uses bound values and validated identifiers. Database constraints remain the final protection against concurrent writes.

## Files

Uploaded files can be validated for file type, image requirements, size, and related upload constraints before storage.

## Custom rules

Applications can register reusable validation rules for domain-specific constraints.

## Bail

The `bail` rule stops validation of a field after its first failure.

## Validation results

Lower-level validation APIs can return a result containing validated values and field-specific error messages instead of throwing.
