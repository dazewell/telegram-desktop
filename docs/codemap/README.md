# Codemap

The codemap stores small, durable facts that make future repository work more
accurate:

- `ui-to-code.md` identifies where a visible interaction really enters code.
- `upstream-traps.md` records non-obvious upstream behavior likely to cause
  future mistakes.
- `dead-ends.md` records investigated and disproven hypotheses.

Every entry requires a current `file:line` citation and the date established.
Readers must re-verify citations before relying on them because upstream code
moves. A durable discovery lands with the same change that established it.

This is not a general investigation log. Do not record guesses, transient
debugging notes, broad architecture summaries, or facts obvious from a file
name. Remove or update entries that no longer verify.
