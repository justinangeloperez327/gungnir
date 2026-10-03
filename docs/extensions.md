# Packages and Extensions

## Overview
Providers register services and participate in application boot/shutdown. The extension Registry stores explicit Plugin instances by package name and rejects duplicate registrations. A plugin supplies package metadata and a provider.

## Scope
This interface does not implement dynamic library discovery/loading, dependency solving or a package manager. Header/interface presence alone is not a tested installable plugin workflow. Validate a plugin against the exact framework commit and native toolchain.



- [include/gungnir/core/provider.hpp](../include/gungnir/core/provider.hpp)
- [include/gungnir/extensions/plugin.hpp](../include/gungnir/extensions/plugin.hpp)
- [include/gungnir/extensions/extensions.hpp](../include/gungnir/extensions/extensions.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/extensions.md).
