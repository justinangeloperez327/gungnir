# Validation

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/validation.md).

## Current behavior

`Validator::check` returns `Result { values, errors }`; `Validator::validate` throws `ValidationException` on failure. Request methods use these contracts. The legacy overload uses string maps. JSON overloads return `StructuredResult` and preserve scalar, array, object and null types. Native request methods are `structured_input`, `check_structured` and `validate_structured`; structured `.gnr` controllers use `request.structuredInput()` and `request.validate({...})`. Errors map each field to a vector of messages.

The current rule names are `required`, `present`, `nullable`, `sometimes`, `string`, `integer`, `numeric`, `boolean`, `email`, `accepted`, `length`, `min`, `max`, `in`, `same`, `confirmed`, `array`, `object`, `bail`, `unique` and `exists`. Numeric `min`/`max` semantics depend on the numeric rules on that field.

Nested fields use dot paths; array elements use complete wildcard segments such as `users.*.email`. Selecting a parent object validates and returns that subtree; select child paths when only those fields should be returned. SQL `unique:table,column` and `exists:table,column` use bound values and checked identifiers on the active connection. `unique:table,column,ignored_id,id_column` supports updates. These checks supplement database constraints and cannot eliminate concurrent-write races. Unknown rules are rejected before inspecting optional or missing fields. See the native validator for exact value parsing and error messages.

## Example

```gnr
controller FormController {
    Response store(Request& request) {
        const data = request.validate({
            "email": "required|email"
        });
        return json(data);
    }
}
```

## Limits and planned work

URL/UUID/date, file/image, conditional-required rules and custom-rule registration remain planned. Structured validation preserves the supplied JSON types; it does not coerce numeric strings to numbers. Database rules currently require SQL connections.

## Implementation references

- [include/gungnir/validation/rules.hpp](../include/gungnir/validation/rules.hpp)
- [include/gungnir/validation/result.hpp](../include/gungnir/validation/result.hpp)
- [src/validation/validator.cpp](../src/validation/validator.cpp)
- [src/language/validation_lowering.cpp](../src/language/validation_lowering.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/validation.md).
