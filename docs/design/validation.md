# Validation

> **Future design specification.** This document describes intended long-term architecture beyond the published `1.0` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../validation.md) before using an API.


## Release alignment

This document describes possible future architecture beyond 1.0. Current implementation guides and executable tests define the published contract.

Gungnir validation verifies that incoming or application data satisfies declared rules before the data is used.

The normal controller flow is:

```gnr
const data = request.validate({
    'name': 'required|string',
    'email': 'required|email'
});
```

If validation succeeds, `data` contains validated values.

If validation fails, normal action execution stops and the framework produces the configured validation error response.

Gungnir follows familiar Laravel-style validation rule naming where those rules map cleanly to the framework.

# Validation responsibility

Validation answers:

```text
Is this field present when required?
Is the value the expected type?
Does the value satisfy length/range rules?
Does it match one of the allowed values?
Does it match another field?
Does it exist in or remain unique within persistence?
Are nested structures valid?
```

Validation does not answer:

```text
Is the current user authorized?
Should the business operation be allowed?
How should the record be persisted?
```

Those concerns belong to authorization, services, models, and the ORM.

# Basic validation

```gnr
const data = request.validate({
    'name': 'required|string',
    'email': 'required|email',
    'age': 'nullable|integer'
});
```

The validation rule string uses `|` to compose rules for one field.

```text
required|string|max:100
```

Rules are parsed into structured validation rules by the compiler/runtime.

# Validated data

After successful validation:

```gnr
const data = request.validate({
    'name': 'required|string',
    'email': 'required|email'
});

const user = User::create(data);
```

The returned value should contain the fields that were part of the validation contract, not arbitrary unvalidated request data.

This makes:

```gnr
User::create(data);
```

a clear validated-input flow.

Model `fillable` protection still applies independently.

# Required

```gnr
'name': 'required'
```

The field must be present and contain an acceptable value according to the rule contract.

# Present

```gnr
'name': 'present'
```

The field must exist in the input even if its value is empty or null where other rules permit that value.

# Nullable

```gnr
'middle_name': 'nullable|string'
```

If the field value is null, subsequent non-presence rules may be skipped according to validation semantics.

# Sometimes

```gnr
'name': 'sometimes|string|max:100'
```

The rule applies only when the field is present.

This is useful for partial updates.

# String

```gnr
'name': 'required|string'
```

# Integer

```gnr
'age': 'required|integer'
```

# Numeric

```gnr
'price': 'required|numeric'
```

# Boolean

```gnr
'active': 'required|boolean'
```

Accepted boolean representations must be deterministic and documented by the validation/runtime contract.

# Array

```gnr
'tags': 'required|array'
```

# Object/map

Where object validation is required:

```gnr
'address': 'required|object'
```

# Email

```gnr
'email': 'required|email'
```

Email validation should verify a syntactically acceptable address. It does not prove mailbox ownership or deliverability.

# URL

```gnr
'website': 'nullable|url'
```

# UUID

```gnr
'public_id': 'required|uuid'
```

# Date

```gnr
'birth_date': 'required|date'
```

# Date time

```gnr
'published_at': 'nullable|datetime'
```

# Min and max

For strings, numbers, arrays, and other supported types:

```gnr
'name': 'required|string|min:2|max:100'
```

```gnr
'age': 'required|integer|min:18|max:120'
```

The interpretation of `min` and `max` depends on the validated type.

# Length

Exact length:

```gnr
'country_code': 'required|string|length:2'
```

# In

```gnr
'status': 'required|in:draft,published,archived'
```

# Not in

```gnr
'username': 'required|notIn:admin,root,system'
```

# Same

```gnr
'email_confirmation': 'required|same:email'
```

# Different

```gnr
'new_password': 'required|different:old_password'
```

# Confirmed

```gnr
'password': 'required|string|confirmed'
```

The conventional confirmation field is:

```text
password_confirmation
```

# Accepted

```gnr
'terms': 'accepted'
```

Accepted values must be defined consistently by the validation runtime.

# Required if

Conditional requirements may use structured or rule-string syntax where practical.

Simple form:

```gnr
'company_name': 'requiredIf:account_type,company|string'
```

For complex conditions, Gungnir should prefer structured validation expressions over increasingly complex mini-language strings.

# Nested data

Nested fields use dotted paths:

```gnr
const data = request.validate({
    'name': 'required|string',
    'address.street': 'required|string',
    'address.city': 'required|string',
    'address.country': 'required|string'
});
```

The validated result should preserve the nested structure.

# Array items

Wildcard paths may validate array items:

```gnr
const data = request.validate({
    'items': 'required|array',
    'items.*.product_id': 'required|integer',
    'items.*.quantity': 'required|integer|min:1'
});
```

This is important for APIs that submit repeated nested records.

# File validation

Uploaded files should support validation rules such as:

```gnr
const data = request.validate({
    'avatar': 'required|file',
    'document': 'file|max:10240'
});
```

Additional file rules may include:

```text
file
image
mimes
mimeTypes
max
min
```

Exact file-size units must be documented and consistent.

# Image validation

```gnr
'avatar': 'required|image'
```

Image validation should verify file characteristics through the upload/file layer rather than trust only the client-supplied MIME type.

# MIME validation

```gnr
'document': 'required|file|mimes:pdf,docx'
```

Gungnir should distinguish extension-style rules from actual MIME/content checks where relevant.

# Database existence

Use `exists` for values that must reference an existing record:

```gnr
'user_id': 'required|integer|exists:users,id'
```

The database-backed validation layer must use parameterized queries.

# Database uniqueness

```gnr
'email': 'required|email|unique:users,email'
```

Database uniqueness validation is useful for user feedback, but database unique constraints remain the final concurrency-safe guarantee.

Applications should still define a unique database constraint in the migration:

```gnr
table.string('email').unique();
```

# Unique rule during updates

An update must be able to ignore the current record.

Gungnir should support a structured form rather than forcing complex dynamic strings.

Conceptually:

```gnr
const data = request.validate({
    'email': Rule::unique('users', 'email')
        .ignore(user.id)
});
```

This allows the compiler/runtime to represent validation configuration structurally.

# Structured rules

Simple rules may use concise strings:

```gnr
'email': 'required|email'
```

More complex rules should support rule objects:

```gnr
'email': [
    Rule::required(),
    Rule::email(),
    Rule::unique('users', 'email').ignore(user.id)
]
```

The two forms should lower to the same validation rule representation.

This avoids pushing complex configuration into fragile strings.

# Custom messages

Validation may provide custom messages:

```gnr
const data = request.validate(
    {
        'email': 'required|email'
    },
    messages: {
        'email.required': 'Email is required.',
        'email.email': 'Enter a valid email address.'
    }
);
```

Message keys should use a stable:

```text
field.rule
```

convention.

# Custom attribute names

Human-readable names may be supplied:

```gnr
const data = request.validate(
    {
        'email_address': 'required|email'
    },
    attributes: {
        'email_address': 'email address'
    }
);
```

Localization of validation messages belongs to the localization layer.

# Validation errors

Validation failures should produce structured errors.

Conceptually:

```text
{
    "email": [
        "The email field is required."
    ],
    "password": [
        "The password must be at least 8 characters."
    ]
}
```

Application code should be able to consume errors by field rather than parse one concatenated message string.

# HTTP validation failure

For a JSON/API request, validation failure should normally produce an appropriate client-error response, conventionally HTTP 422.

For browser/web requests, the configured web behavior may redirect back with errors and prior input.

The response strategy belongs to the HTTP/application layer, while the validation system provides the structured validation failure.

# Check without throwing/short-circuiting

Gungnir may expose a non-throwing validation flow for cases where code needs to inspect the result manually:

```gnr
const result = request.check({
    'email': 'required|email'
});

if (result.failed()) {
    return json({
        'errors': result.errors()
    }, 422);
}
```

Normal controller code should prefer:

```gnr
request.validate(...)
```

when automatic failure handling is appropriate.

# Validating non-request data

Validation should not be tied exclusively to HTTP.

A general validator may validate arbitrary structured data:

```gnr
const data = Validator::validate(payload, {
    'name': 'required|string',
    'email': 'required|email'
});
```

This allows application services, command handlers, jobs, or imports to reuse validation rules without fabricating a Request object.

# Dedicated validated requests

A future dedicated request-validation declaration may be supported for large reusable rule sets.

For example, if Gungnir introduces a first-class validated request type, it should remain an explicit language/framework contract rather than expose native C++ inheritance boilerplate.

Until that contract is finalized, controller-local:

```gnr
request.validate(...)
```

is the canonical syntax.

# Validation and authorization

Validation and authorization are separate.

This:

```gnr
request.validate({
    'title': 'required|string'
});
```

answers whether the input is valid.

A policy answers whether the current actor may perform the operation.

Do not use validation rules as a substitute for authorization.

# Validation and database constraints

Validation improves application feedback.

Database constraints enforce persistence integrity.

For example:

```gnr
'email': 'unique:users,email'
```

should normally be paired with:

```gnr
table.string('email').unique();
```

The validator cannot eliminate race conditions between validation and persistence.

# Validation and model fillable

These are also separate:

```gnr
const data = request.validate({
    'name': 'required|string',
    'email': 'required|email'
});

User::create(data);
```

Model:

```gnr
model User {
    fillable = [
        'name',
        'email'
    ];
}
```

Validation says the input is acceptable.

`fillable` says the attributes may be mass assigned.

# Validation and casts

Validation checks incoming values.

Model casts describe persisted/application representation.

For example:

```gnr
'active': 'required|boolean'
```

and:

```gnr
casts = {
    'active': 'bool'
};
```

solve different problems.

# Bail behavior

A field may stop evaluating additional rules after the first failure:

```gnr
'email': 'bail|required|email|unique:users,email'
```

This can avoid unnecessary expensive database-backed rules when an earlier rule already failed.

# Stop on first failure

For operations that intentionally stop globally after the first validation error, a structured option may be supported:

```gnr
Validator::validate(
    payload,
    rules,
    stopOnFirstFailure: true
);
```

Normal form validation should generally collect useful field errors.

# Custom validation rules

Applications should be able to define reusable custom rules without modifying the framework.

The exact custom-rule declaration syntax should be finalized separately.

The contract should require that a custom rule:

```text
receives a value and validation context
returns success/failure or a structured validation result
produces a stable rule identity/message key
```

It should not require application developers to inherit from template-heavy native C++ classes.

# Validation security

Validation input remains untrusted until validated.

Even validated strings are not automatically safe for every output context.

Validation does not replace:

```text
HTML escaping
SQL parameter binding
authorization
CSRF protection
file-content security checks
output encoding
```

# What does not belong in validation

Validation rules should not contain:

- business workflows;
- side-effect-heavy application logic;
- mail sending;
- unrelated persistence;
- authorization decisions;
- controller routing logic;
- schema migration behavior.

Database-backed rules may query persistence only for validation purposes such as `exists` or `unique`.

# Core validation rule reference

The intended core rule set includes:

| Rule | Purpose |
| --- | --- |
| `required` | Value must be present and validly non-empty |
| `present` | Field must exist |
| `nullable` | Null is allowed |
| `sometimes` | Validate only when present |
| `bail` | Stop rules for this field after first failure |
| `string` | String value |
| `integer` | Integer value |
| `numeric` | Numeric value |
| `boolean` | Boolean-compatible value |
| `array` | Array/list value |
| `object` | Object/map value |
| `email` | Email syntax |
| `url` | URL syntax |
| `uuid` | UUID syntax |
| `date` | Date-compatible value |
| `datetime` | Date-time-compatible value |
| `min` | Minimum value/length/count |
| `max` | Maximum value/length/count |
| `length` | Exact length/count |
| `in` | Value in allowed set |
| `notIn` | Value not in forbidden set |
| `same` | Same as another field |
| `different` | Different from another field |
| `confirmed` | Matches conventional confirmation field |
| `accepted` | Accepted/affirmative value |
| `requiredIf` | Conditionally required |
| `file` | Uploaded file |
| `image` | Uploaded image |
| `mimes` | Allowed file extensions/types |
| `exists` | Referenced database value exists |
| `unique` | Database value is unique |

Additional rules may be added without changing the overall validation architecture.

# Validation AST

A validation expression should be represented structurally.

For:

```gnr
request.validate({
    'email': 'required|email|max:255'
});
```

the compiler/runtime should conceptually resolve:

```text
ValidationCall
  source       request
  fields
    email
      RequiredRule
      EmailRule
      MaxRule
        value = 255
```

Complex structured rule expressions should resolve to the same internal rule representation.

# Semantic validation

The validation semantic pass should validate where possible:

- rule names are known;
- rule argument counts are valid;
- rule arguments have valid types;
- incompatible rule combinations are diagnosed where deterministic;
- dotted/wildcard paths are syntactically valid;
- structured `Rule::...` calls resolve;
- database rule arguments are structurally valid;
- custom message keys follow valid field/rule naming;
- `request.validate()` is called on a Request-compatible value;
- `Validator::validate()` receives structured data.

Runtime data-dependent validation remains a runtime concern.

# Compiler contract

This source:

```gnr
const data = request.validate({
    'name': 'required|string|max:100',
    'email': 'required|email|unique:users,email'
});
```

should conceptually pass through:

```text
source
  -> parser
  -> object expression AST
  -> validation call resolution
  -> rule parsing/normalization
  -> validation-rule AST
  -> semantic validation
  -> validated rule representation
  -> validation lowering
  -> C++23/runtime execution
```

Before native generation, the compiler/runtime representation should know:

```text
field      name
rules      required, string, max(100)

field      email
rules      required, email, unique(users, email)
```

The transpiler must not rely on ad-hoc raw source scanning to identify validation constructs.

# Database-backed rule contract

Rules such as:

```text
exists
unique
```

must use the database abstraction and parameterized bindings.

They must not concatenate untrusted values into SQL.

The validation core should depend on a database-validation contract rather than a specific PostgreSQL, MySQL, SQL Server, SQLite, or MongoDB driver.

Backend differences must remain explicit where validation semantics cannot be made equivalent.

# Generated C++ boundary

Gungnir source:

```gnr
const data = request.validate({
    'email': 'required|email'
});
```

may lower to native validation rule objects, result containers, exception/error types, or generated metadata.

Application code should not need to construct:

```text
native Rules containers
template validators
error maps
database binding objects
ValidationException plumbing
```

unless intentionally using the lower-level native API.

# Naming convention

Public validation APIs use camelCase where multiple words are required:

```text
notIn
requiredIf
stopOnFirstFailure
```

Simple rule/API names remain concise:

```text
validate
check
required
present
nullable
sometimes
bail
string
integer
numeric
boolean
array
object
email
url
uuid
date
datetime
min
max
length
in
same
different
confirmed
accepted
file
image
mimes
exists
unique
```

# Design rule

The validation contract is intentionally focused:

```text
validation = declared rules + validated data + structured errors
```

Request parsing gathers input.

Validation determines whether that input satisfies application rules.

Authorization determines whether the actor may perform the action.

Models and the ORM persist validated data.

Those responsibilities remain separate.

