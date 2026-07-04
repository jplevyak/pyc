# Issue 023: Structural pattern matching (`match`/`case`, PEP 634)

**Status:** open.
**Affects:** `python.g` and pyc lowering.

## Description

No grammar rule; `match`/`case` aren't even reserved words today (they're soft keywords in real Python 3.10+, contextual on appearing at statement-start before a `:`). This is a substantial addition — full pattern matching (literal patterns, capture patterns, class patterns with attribute binding, or-patterns, guards) is comparable in scope to a small compiler feature of its own; a minimal subset (literal + capture patterns only, no class patterns) would be a reasonable first slice.

## Verification plan
1. match/case: minimal literal+capture pattern test.
2. Add one test file for match/case once implemented.
