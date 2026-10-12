# Spear example

Create a normal application with `gungnir new SpearExample`, then copy this
directory's `app`, `routes`, `views` and `bootstrap` directories into it. Run
`gungnir dev` from the application directory and visit `http://127.0.0.1:8000`.
No database is required.

The example renders models using a layout chain, escaped sections, includes,
loop metadata, empty states, conditions, a registered helper and explicitly
trusted raw HTML. `/legacy` selects the existing `.html` grammar explicitly;
`/numbers` preserves signed and unsigned 64-bit values. Hidden model attributes
are excluded before rendering. See the [view guide](https://github.com/justinangeloperez327/gungnir/blob/main/docs/view.md).

Edit any `.spear` template while `gungnir dev` runs to see it on the next
request. A production process (`APP_ENV=production`) caches parsed templates
until restart or explicit `app.views().clear_cache()` invalidation.
