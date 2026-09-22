# Controllers

Controllers are application-facing request handlers resolved through the Gungnir container. They should contain HTTP orchestration, not database/runtime plumbing.

A controller derives from `Controller`. The Gungnir language lowerer makes controller members public and generates constructor wiring for declared `inject Type name;` dependencies. Native C++ controller routes continue to use explicit member pointers.

Controller actions may accept `Request&` or no request argument. Actions may return `Response`, `Task<Response>`, `std::string`, or `std::string_view`. String results normalize to text responses; asynchronous mechanics remain hidden by Gungnir source syntax.

The base controller provides response, text, JSON, view, HTML, download, redirect and no-content helpers.

Supported controller route verbs are GET, POST, PUT, PATCH, DELETE, OPTIONS and HEAD.

Model/entity parameters are not fabricated in the controller layer. Typed route-model binding must be completed against Gungnir ORM so an action such as `show(User user)` has deterministic lookup, missing-model and ownership semantics.

Validation-specific request classes belong to Group 11. Controllers should consume validated request abstractions once that layer is available rather than embedding validation plumbing into every action.
