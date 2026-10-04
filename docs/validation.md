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

Successful validation returns a `Json` object with selected values in their original types. A numeric string remains a string. Invalid input raises `ValidationException`; the HTTP exception handler returns status 422 with field-specific errors, without submitted values. Malformed JSON or multipart input produces status 400.

## Rules

Combine rules with `|`. Literal definitions are checked during compilation. Dynamic definitions receive the same checks at runtime, including rules for absent fields. Invalid names, paths, identifiers, arguments, or duplicate field definitions are configuration errors.

| Rule | Contract |
| --- | --- |
| `required` | Present and neither null, blank string, nor empty array/object. |
| `present` | The key exists; other rules determine which values are permitted. |
| `nullable` | Null bypasses non-presence rules. |
| `sometimes` | An absent field bypasses all its rules. |
| `string`, `array`, `object` | Corresponding JSON type. |
| `integer` | JSON integer or exact signed/unsigned 64-bit integer string; excludes booleans and floating values. |
| `numeric` | Finite JSON number or numeric string; excludes booleans. |
| `boolean` | Boolean, integer 0/1, or string `true`, `false`, `1`, `0`, `yes`, `no`, `on`, `off`. |
| `accepted` | Boolean true, integer 1, or string `true`, `1`, `yes`, `on`. |
| `email` | ASCII dot-atom address with dotted domain; does not check deliverability. |
| `uuid` | 36-character hexadecimal UUID with hyphens, including nil UUIDs. |
| `url` | Absolute HTTP/HTTPS URL with DNS, IPv4, or bracketed IPv6 host; excludes credentials and invalid percent escapes. |
| `date` | Valid Gregorian `YYYY-MM-DD`, years 0001–9999. |
| `datetime` | Date, `T`, `HH:MM:SS`, optional fractional seconds, and `Z` or signed `HH:MM` offset. Seconds 00–59; lowercase `t`/`z` accepted. |
| `min:N`, `max:N`, `size:N` | Lower/upper/exact numeric value with `integer`/`numeric` or JSON numbers; otherwise string bytes, array/object count, or upload bytes. Signed/fractional numeric bounds accepted. |
| `length:N` | Exact string byte length, array/object count, or upload bytes; N is a nonnegative 64-bit integer. |
| `in:a,b`, `notIn:a,b` | Scalar spelling belongs/does not belong to the list. |
| `same:path`, `different:path` | Match/differ from an existing field without converting its JSON type to a string. |
| `confirmed` | Match the field suffixed with `_confirmation`. |
| `requiredIf:path,value` | Required when another scalar field's spelling matches a listed value. |
| `file`, `image` | Actual request upload / supported image container. |
| `mimes:png,jpeg`, `mimetypes:image/png` | Upload bytes match an allowed detected format/media type. |
| `extensions:png,jpg` | Client filename has an allowed extension; combine with content rules. |
| `unique:table,column`, `exists:table,column` | Bound SQL uniqueness/existence checks. |
| `custom:name` | A rule on an explicitly passed registry. |
| `bail` | Stop the field after its first failure. |

Aliases `not_in`, `required_if`, and `dateTime` are accepted. String sizes measure UTF-8 bytes. Validation preserves values without coercion.

## Optional values

Use `nullable` for null and `sometimes` for partial updates.

```gnr
const data = request.validate({
    "nickname": "sometimes|nullable|string|max:40",
    "contact": "requiredIf:method,email|email",
    "method": "required|in:email,sms"
});
```

## Nested data

Dot notation selects nested fields:

```gnr
const data = request.validate({
    "profile": "required|object",
    "profile.name": "required|string",
    "profile.email": "required|email"
});
```

A container with descendant rules returns only selected descendants. A container declared without descendant rules is selected in full. Declare child fields before passing nested data into a write operation.

## Arrays and wildcards

```gnr
const data = request.validate({
    "users": "required|array|min:1|max:20",
    "users.*.email": "required|email",
    "users.*.confirmation": "required|same:users.*.email"
});
```

Wildcards expand array elements. Errors use concrete paths such as `users.1.email`; selected arrays retain their element indexes. A wildcard in a comparison path refers to the corresponding element. Require the parent array when its presence matters.

## Database rules

```gnr
const data = request.validate({
    "email": "required|email|unique:users,email",
    "project_id": "required|integer|exists:projects,id"
});
```

These rules use the active application's SQL connection, bound values and validated identifiers. The column defaults to the field name; specify it for nested/wildcard fields. `unique:users,email,7,id` excludes the row whose `id` is 7. Database constraints remain the final protection against concurrent writes.

## Files

File rules inspect multipart uploads held by the request. JSON metadata cannot satisfy upload rules. Selected upload values contain `name`, client `contentType`, and `size`; obtain bytes through the request upload API after validation.

```gnr
const data = request.validate({
    "avatar": "bail|required|file|image|mimes:png,jpeg|max:1048576"
});
const avatar = request.file("avatar");
if (avatar != null) {
    const bytes = avatar.bytes();
}
```

`image` recognizes PNG, JPEG, GIF, and still WebP containers. It checks headers and bounds, including PNG chunk checksums; it does not decode pixels. `mimes` supports `png`, `jpg`, `jpeg`, `gif`, `webp`, `pdf`, `txt`, and `bin`. PDF detection checks framing; text requires UTF-8 text bytes; `bin` selects otherwise unrecognized binary content. Detected types are independent of client extensions and Content-Type headers.

Use a multipart field named `photos[]` for repeated files, then validate each element:

```gnr
const data = request.validate({
    "photos": "required|array|max:5",
    "photos.*": "bail|required|file|image|max:1048576"
});
const photos = request.files("photos");
```

See [request uploads](request.md#uploads) and [storing uploads](storage.md#uploads).

## Custom rules

Create an owning registry and pass it to validation. Predicates are synchronous, take the original `Json` value and complete input object, and return `bool`. Messages are fixed registration text. Copies share their registry; independently created registries remain separate. Duplicate or missing registrations are configuration errors.

```gnr
function Validator projectRules(string expected) {
    const rules = Validator::make();
    rules.extend("projectCode", "The project code is invalid.",
        (Json value, Json data) => value.isString() && value.string() == expected);
    return rules;
}

controller ProjectController {
    store(Request request) {
        const rules = projectRules("GNR");
        const data = request.validate({"code": "required|custom:projectCode"}, rules);
        return json(data);
    }
}
```

Named synchronous functions with this signature can also be registered. A registry supports `rules.validate(payload, definitions)` and `rules.check(payload, definitions)` for application data.

## Bail

`bail` stops a field after its first failure, including container and upload rules. Other fields continue validation.

## Validation results

`request.check` returns `ValidationResult` without throwing on invalid data. Use `valid()`/`failed()`, `values()` for selected successful fields, and `errors()` for `Map<string, List<string>>`. Check failure before writes.

```gnr
const result = request.check({"email": "bail|required|email"});
const failed = result.failed();
const errors = result.errors();
const values = result.values();
```

Use `Validator::validate` or `Validator::check` for a JSON object outside a request. Use a registry instance for custom rules.

```gnr
const payload = request.json();
const result = Validator::check(payload, {"count": "required|integer|min:0"});
```

The native SDK retains string-map `Validator` and `Request::validate/check` APIs. Structured callers use `Validator::check(Json, Rules)` and `Request::validate_structured/check_structured`. Legacy string-map `nullable` also accepts blank strings.
