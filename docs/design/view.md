# Views

> **Development design specification.** This document describes intended long-term architecture beyond the frozen `development` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../view.md) before using an API.


## Development alignment

Gungnir is under active development. No 1.0 compatibility contract is frozen; this document may describe intended future architecture, but current implementation guides and executable tests remain authoritative.

Gungnir views provide server-rendered HTML templates.

Controllers return views through the normal response helper:

~~~gnr
public index() {
    const users = User::orderBy('name').get();

    return view('users/index', {
        'users': users
    });
}
~~~

Views are presentation templates. They receive data from the controller and render output.

They do not perform routing, database queries, authentication, authorization, or business workflows.

# View responsibility

A view answers:

~~~text
What HTML should be rendered?
Which supplied values should appear?
Which presentation conditions should be shown?
How should collections be repeated?
Which layout, partial, or component should be composed?
~~~

The controller prepares application data.

The view renders it.

# View files

Views live under the application's configured view root.

The conventional directory is:

~~~text
views/
~~~

Example:

~~~text
views/
├── home.html
├── users/
│   ├── index.html
│   └── show.html
├── partials/
│   └── navigation.html
└── layouts/
    └── app.html
~~~

A controller may render:

~~~gnr
return view('users/index');
~~~

When no extension is supplied, Gungnir resolves the conventional .html extension:

~~~text
users/index
    -> views/users/index.html
~~~

Application code should normally omit the extension.

# View root security

View names are logical template names, not arbitrary filesystem paths.

The renderer must reject:

- absolute paths;
- parent traversal such as ../;
- embedded NUL characters;
- paths that escape the configured view root;
- unsafe symbolic-link traversal.

This should be rejected:

~~~gnr
view('../../etc/passwd');
~~~

The view engine owns safe template resolution.

# Passing data to a view

Pass an object as the second argument:

~~~gnr
return view('users/show', {
    'user': user
});
~~~

Multiple values:

~~~gnr
return view('users/index', {
    'title': 'Users',
    'users': users,
    'total': users.count()
});
~~~

The object keys become top-level template variables.

# Escaped interpolation

Render a value with:

~~~html
<h1>{{ title }}</h1>
~~~

Values rendered with double braces are HTML escaped by default.

For example, a value containing HTML or script markup must render as text rather than executable markup.

Escaping by default is a core security rule.

# Raw output

Trusted HTML may be rendered explicitly with triple braces:

~~~html
{{{ trustedHtml }}}
~~~

Raw output bypasses HTML escaping.

It must not be used with untrusted user input.

Prefer:

~~~html
{{ comment.body }}
~~~

not:

~~~html
{{{ comment.body }}}
~~~

unless the value has passed through a deliberate trusted-HTML or sanitization contract.

# Dot paths

Nested object values use dotted paths:

~~~html
<h1>{{ user.name }}</h1>
<p>{{ user.profile.bio }}</p>
~~~

The renderer resolves the path through supplied structured data.

Missing-value behavior must be deterministic. Normal interpolation may render a missing value as empty unless strict-view mode is enabled.

# Models in views

Models may be supplied directly:

~~~gnr
return view('users/show', {
    'user': user
});
~~~

Template:

~~~html
<h1>{{ user.name }}</h1>
<p>{{ user.email }}</p>
~~~

Model-to-view conversion must respect model metadata. Sensitive hidden attributes must not become casually exposed because a model was passed to a view.

# Collections in views

Collections may be supplied directly:

~~~gnr
return view('users/index', {
    'users': users
});
~~~

Iterate with each:

~~~html
<ul>
{{#each users}}
    <li>{{ name }}</li>
{{/each}}
</ul>
~~~

Within an each block, the current element becomes the current template context.

# Current item

For scalar collections:

~~~html
{{#each tags}}
    <span>{{ this }}</span>
{{/each}}
~~~

For object or model collections:

~~~html
{{#each users}}
    <strong>{{ name }}</strong>
{{/each}}
~~~

# Nested loops

Loops may be nested:

~~~html
{{#each users}}
    <h2>{{ name }}</h2>

    <ul>
    {{#each posts}}
        <li>{{ title }}</li>
    {{/each}}
    </ul>
{{/each}}
~~~

Nested blocks must be parsed structurally rather than handled with fragile one-pass replacement.

# Loop metadata

The target contract should expose deterministic loop metadata:

~~~html
{{#each users}}
    <span>{{ loop.index }}</span>
    <span>{{ name }}</span>
{{/each}}
~~~

Useful metadata may include:

~~~text
loop.index
loop.first
loop.last
loop.count
~~~

Gungnir should use zero-based loop.index unless a separate human-oriented ordinal is introduced.

# Conditionals

Conditional rendering should use explicit parsed blocks:

~~~html
{{#if user.active}}
    <span>Active</span>
{{/if}}
~~~

With an else branch:

~~~html
{{#if user.active}}
    <span>Active</span>
{{else}}
    <span>Inactive</span>
{{/if}}
~~~

Conditions follow Gungnir view truthiness rules rather than native C++ implicit-conversion rules.

# Unless

A negative conditional may be supported for readability:

~~~html
{{#unless users}}
    <p>No users found.</p>
{{/unless}}
~~~

If unless does not provide enough value, if plus negation may remain the canonical mechanism.

# Empty collection rendering

~~~html
{{#if users}}
    <ul>
    {{#each users}}
        <li>{{ name }}</li>
    {{/each}}
    </ul>
{{else}}
    <p>No users found.</p>
{{/if}}
~~~

Collection truthiness must be deterministic.

# Partials

Reusable fragments should use explicit partial syntax:

~~~html
{{> 'partials/navigation' }}
~~~

A partial may receive explicit data:

~~~html
{{> 'users/card' user=user }}
~~~

Partials use the same safe view-root resolution rules as full views.

# Layouts

Views should support layouts without controller-side HTML composition.

Target syntax:

~~~html
{{#layout 'layouts/app'}}

{{#section 'title'}}
Users
{{/section}}

{{#section 'content'}}
<h1>Users</h1>

{{#each users}}
    <p>{{ name }}</p>
{{/each}}
{{/section}}

{{/layout}}
~~~

A layout may render sections:

~~~html
<!doctype html>
<html>
<head>
    <title>{{#yield 'title'}}Gungnir{{/yield}}</title>
</head>
<body>
    {{#yield 'content'}}{{/yield}}
</body>
</html>
~~~

Layout syntax must be implemented through a real template parser and AST before it is marked stable.

# Components

Reusable presentation components should be explicit:

~~~html
{{#component 'components/alert' type='error'}}
    Unable to save the record.
{{/component}}
~~~

A component may receive props and content/slot data.

Components are presentation composition, not hidden application-service containers.

# Comments

Template-only comments should not appear in output:

~~~html
{{! This comment is not rendered }}
~~~

HTML comments remain ordinary HTML:

~~~html
<!-- This is sent to the browser -->
~~~

# View expressions

View expressions should remain intentionally limited.

Supported expressions should focus on presentation:

~~~text
variable paths
boolean conditions
simple comparisons
safe helper calls
loop metadata
~~~

Templates should not become a second unrestricted programming language.

Complex calculations belong in controllers, view-data builders, or application services.

# Comparisons

Conditionals may support simple comparisons:

~~~html
{{#if user.role == 'admin'}}
    <a href="/admin">Admin</a>
{{/if}}
~~~

The expression grammar must be explicitly parsed rather than evaluated as native C++ or arbitrary script text.

# View helpers

Small presentation helpers may be exposed:

~~~html
{{ formatDate(user.created_at) }}
{{ route('users.show', {'user': user.id}) }}
~~~

Only registered safe view helpers should be callable.

Views must not be able to invoke arbitrary native C++ functions.

# URL generation

Views may generate named-route URLs:

~~~html
<a href="{{ route('users.show', {'user': user.id}) }}">
    {{ user.name }}
</a>
~~~

The routing layer owns URL generation semantics.

# Assets

A presentation helper may resolve public assets:

~~~html
<link rel="stylesheet" href="{{ asset('css/app.css') }}">
~~~

Asset URL generation belongs to the application/public-file configuration layer.

# CSRF fields

Server-rendered forms using session authentication should use a CSRF helper:

~~~html
<form method="post" action="/users">
    {{ csrfField() }}

    <input name="name">
</form>
~~~

The CSRF system owns token generation and validation.

The view helper only renders the form field.

# Method spoofing

HTML forms support only GET and POST directly.

Where application routing permits PUT, PATCH, or DELETE through browser forms:

~~~html
{{ methodField('delete') }}
~~~

Example:

~~~html
<form method="post" action="/users/{{ user.id }}">
    {{ csrfField() }}
    {{ methodField('delete') }}

    <button type="submit">Delete</button>
</form>
~~~

The HTTP middleware/router owns method-override semantics.

# Validation errors

Views should be able to render structured validation errors supplied through the normal web/session flow:

~~~html
{{#if errors.email}}
    <p>{{ errors.email.first }}</p>
{{/if}}
~~~

Validation defines the errors. The view only presents them.

# Previous input

Browser form workflows may expose previous input through a presentation helper:

~~~html
<input
    name="email"
    value="{{ old('email') }}"
>
~~~

This belongs to request/session flash-data integration.

# Escaping contexts

HTML text interpolation is escaped by default.

Different contexts require different encoding rules:

~~~text
HTML text
HTML attributes
URLs
JavaScript
CSS
~~~

Gungnir should not claim one generic escaping operation safely covers every context.

Context-aware helpers or explicit safe encoders should be provided before those contexts are treated as secure.

# JavaScript data

Prefer a dedicated JSON/script encoder:

~~~html
<script>
    const user = {{{ jsonForScript(user) }}};
</script>
~~~

Do not encourage ordinary string interpolation as a universal JavaScript-data solution.

# Views should not query the database

Avoid database or ORM calls in templates.

Prepare data in the controller:

~~~gnr
public index() {
    const users = User::with('profile')
        .orderBy('name')
        .get();

    return view('users/index', {
        'users': users
    });
}
~~~

Then render:

~~~html
{{#each users}}
    <p>{{ name }}</p>
{{/each}}
~~~

This keeps I/O visible and avoids hidden N+1 queries during rendering.

# Views should not enforce authorization

A view may hide or show presentation based on authorization information, but hiding a button does not secure an operation.

Controllers and policies must enforce authorization independently.

# View response

The view helper returns a response-compatible value:

~~~gnr
public index() {
    return view('users/index', {
        'users': User::all()
    });
}
~~~

Conceptually:

~~~text
view(...)
    -> ViewResponse
    -> Response-compatible
~~~

# Missing views

A missing template should raise a view-specific not-found/rendering error.

It must not silently return an empty page.

# Syntax errors

Malformed template syntax should produce a view-specific error with useful source information:

~~~text
template
line
column
message
~~~

Production responses may hide internal filesystem details.

# Template compilation

The final architecture should parse templates into a structured representation:

~~~text
.html template
    -> view lexer
    -> view parser
    -> Template AST
    -> semantic validation
    -> render plan / compiled representation
    -> HTML response
~~~

Gungnir should not grow the template language indefinitely through ad-hoc source replacement.

# Template AST

A target representation may include:

~~~text
Template
  nodes[]

TextNode
EscapedInterpolation
RawInterpolation
EachBlock
IfBlock
PartialNode
LayoutNode
SectionNode
YieldNode
ComponentNode
HelperCall
CommentNode
~~~

For:

~~~html
{{ user.name }}
~~~

the AST should represent:

~~~text
EscapedInterpolation
  path
    user
    name
~~~

# View data typing

View data originates from typed Gungnir expressions:

~~~gnr
return view('users/show', {
    'user': user,
    'posts': posts
});
~~~

The compiler can conceptually know:

~~~text
user   User
posts  Collection<Post>
~~~

Where project context is available, view tooling should use this metadata to catch obvious invalid attribute access.

Dynamic data remains runtime validated where static information is unavailable.

# Semantic validation

View tooling should validate where possible:

- template paths remain under the configured root;
- block syntax is balanced;
- nested blocks are structurally valid;
- referenced partials, layouts, and components exist when project files are available;
- helper names resolve to approved helpers;
- each targets iterable values when types are known;
- conditions are truthy/boolean-compatible;
- known model attributes are valid;
- raw output remains explicit;
- sections and yields are structurally compatible.

# Runtime ownership

Each application owns its view engine.

Rendering during a request uses the view engine associated with that application/request context.

Async controller execution must preserve this context across suspension.

Application code should not manage native template-engine pointers or global mutable rendering state.

# Caching

Production environments may cache parsed or compiled templates.

Development environments may re-read templates more aggressively.

Caching must not change template semantics.

# View helpers and purity

Good view helpers are presentation-oriented:

~~~text
route(...)
asset(...)
formatDate(...)
csrfField()
methodField(...)
old(...)
~~~

Poor view helpers hide application side effects:

~~~text
chargeCreditCard(...)
deleteUser(...)
sendMail(...)
runDatabaseQuery(...)
~~~

# What does not belong in a view

Views should not contain:

- ORM/database queries;
- business workflows;
- authentication mutation;
- authorization enforcement;
- migrations;
- mail sending;
- event dispatch as application workflow;
- controller routing;
- raw C++ code;
- arbitrary filesystem or network access.

Views render presentation.

# Complete example

Controller:

~~~gnr
controller UserController {
    public index() {
        const users = User::with('profile')
            .orderBy('name')
            .get();

        return view('users/index', {
            'title': 'Users',
            'users': users
        });
    }
}
~~~

View:

~~~html
<h1>{{ title }}</h1>

{{#if users}}
    <ul>
    {{#each users}}
        <li>
            <a href="{{ route('users.show', {'user': id}) }}">
                {{ name }}
            </a>
        </li>
    {{/each}}
    </ul>
{{else}}
    <p>No users found.</p>
{{/if}}
~~~

# Core syntax reference

| Syntax | Purpose |
| --- | --- |
| {{ value }} | Escaped interpolation |
| {{{ value }}} | Explicit raw interpolation |
| {{#each items}}...{{/each}} | Iterate values |
| {{ this }} | Current scalar/item |
| {{ user.name }} | Nested path |
| {{#if condition}}...{{/if}} | Conditional block |
| {{else}} | Alternate branch |
| {{> 'partial' }} | Partial include |
| {{! comment }} | Template-only comment |

Layouts, components, sections, yields, and advanced helpers are part of the target contract but become stable only when the parser and AST support them.

# Current implementation boundary

The existing view runtime already supports:

~~~text
views/ default root
.html default extension
safe root/path resolution
{{ value }} escaped interpolation
{{{ value }}} raw interpolation
dot-path lookup
{{#each ...}} blocks
nested each blocks
model/collection conversion to view values
~~~

The broader contract in this document defines the direction for the view parser. It does not imply every advanced construct is implemented today.

# Compiler and runtime contract

This controller expression:

~~~gnr
return view('users/index', {
    'users': users
});
~~~

should be represented structurally before lowering:

~~~text
ViewExpression
  template = users/index
  data
    users -> Collection<User>
  result = ViewResponse
~~~

The template is parsed separately:

~~~text
views/users/index.html
  -> Template AST
  -> validated render representation
~~~

The transpiler must not rediscover view data through raw source scanning.

# Generated C++ boundary

Application code may lower to native view-data builders, render plans, response objects, typed values, and safe filesystem resolution.

These details are not part of the application-facing API.

# Design rule

The view contract is intentionally focused:

~~~text
view = presentation template + explicitly supplied data
~~~

Controllers prepare data.

Views render presentation.

The renderer escapes output by default.

The template engine must evolve through explicit parsing and AST design rather than increasingly fragile text replacement.

