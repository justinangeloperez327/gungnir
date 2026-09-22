# Packages and Plugins

Gungnir extensions build on the existing Provider lifecycle rather than introducing a second application lifecycle.

## Package metadata

A package declares a stable name, its own version and an optional Gungnir compatibility expression. Metadata is descriptive at this stage; compatibility expressions are not enforced until Gungnir has a defined version-range grammar and parser.

## Plugins

A Plugin exposes package metadata and a Provider. The provider is the integration point for service registration, boot, ready and shutdown behavior.

The Registry owns plugin references, rejects null plugins, validates required metadata and prevents duplicate package names.

## Native C++ interoperability

Extensions remain ordinary C++ libraries. Gungnir does not require a proprietary package archive or registry. CMake-compatible libraries can expose a Plugin or Provider while continuing to use normal C++ package distribution.

## Compatibility

The framework must not claim semantic-version compatibility from an unparsed string. A future compatibility checker should use Gungnir's published stability policy and a documented range grammar.

## Security

Loading arbitrary native plugins executes native code with the application's privileges. Plugin discovery and dynamic loading must therefore remain explicit; the framework does not automatically execute libraries found on disk.
