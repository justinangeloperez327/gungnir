# Groups 03–08 Integration Repair

This branch reconciles the framework state after Groups 03–08 were merged out of dependency order.

The merged main branch already retained the substantive lifecycle, container, asynchronous runtime, HTTP runtime, routing, request and response implementations. The repair therefore does not replay old branches. It repairs cross-group seams in the resulting tree.

Repairs:
- expose the Group 06 and Group 08 HTTP APIs through the public umbrella header;
- expose the Group 07 binding registry;
- restore the HTTP connection serialization policy so the selected keep-alive/close directive is actually emitted;
- add the lifecycle header dependency required by its move operations;
- remove an incomplete route-group naming surface that stored a prefix but never applied it.

Groups 03–08 should be treated as one integrated baseline after this repair is merged. New maturity branches must be created from main only after the preceding integration branch is merged.
