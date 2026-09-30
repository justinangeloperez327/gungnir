# Validation

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/validation.md).

## Current behavior

`Validator::check` returns `Result { values, errors }`; `Validator::validate` throws `ValidationException` on failure. Request methods use these contracts. Inputs and validated values are string maps; errors map each field to a vector of messages.

The current rule names are `required`, `present`, `nullable`, `sometimes`, `string`, `integer`, `numeric`, `boolean`, `email`, `accepted`, `length`, `min`, `max`, `in`, `same` and `confirmed`. Numeric `min`/`max` semantics depend on the numeric rules on that field.

Unknown rules are rejected. See the native validator for exact value parsing and error messages.

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

Database `unique`/`exists`, array/object rules, URL/UUID/date rules, file/image rules, `bail`, conditional required rules and custom-rule registration in the target catalogue are not all implemented. Validation does not currently turn the string map into a fully typed object.

## Implementation references

- [include/gungnir/validation/rules.hpp](../include/gungnir/validation/rules.hpp)
- [include/gungnir/validation/result.hpp](../include/gungnir/validation/result.hpp)
- [src/validation/validator.cpp](../src/validation/validator.cpp)
- [src/language/validation_lowering.cpp](../src/language/validation_lowering.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/validation.md).
