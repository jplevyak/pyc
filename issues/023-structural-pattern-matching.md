# Issue 023: Structural pattern matching (`match`/`case`, PEP 634)

**Status:** open, partial. **Capture patterns fixed 2026-07-12**
(commit — see below); or-patterns' silent miscompile, guards, and
class/sequence/mapping patterns remain.
**Affects:** `python.g` (grammar), `python_ifa_build_syms.cc`
(`PY_case_block` symbol building), `python_ifa_build_if1.cc`
(`build_match_pyda`, lowering).

## Current state (checked 2026-07-12 against the actual code, not assumed)

The original filing said "no grammar rule; not even reserved
words." That's stale — `match_stmt`/`case_block`/`MATCH_KW`/
`CASE_KW` exist in `python.g` (soft-keyword contextual match on
`match`/`case` at statement start), and `build_match_pyda`
(`python_ifa_build_if1.cc:855`) lowers matched statements into an
if-else chain. Actual per-pattern-kind status, verified by
compiling each shape and diffing against `python3`'s real output
(not just checking compile success):

- **Literal patterns** (`case 1:`, `case "foo":`) — **working**,
  byte-identical to CPython. `tests/match_basic.py` covers this
  (literal + wildcard only) and passes.
- **Wildcard** (`case _:`) — **working**.
- **Capture patterns** (`case x:`, PEP 634's most common
  form — bind the subject to a new name) — **FIXED 2026-07-12**.
  Was broken with a hard compile error: `build_syms_pyda`'s
  `PY_case_block` case (`python_ifa_build_syms.cc:768-769`) fell
  into the generic "recurse all children as expression reads"
  bucket alongside `PY_if_stmt`/`PY_with_stmt`/etc. — it never
  treated a case pattern's bare name as a new binding the way an
  assignment target or a `for` loop variable is treated, and
  `build_match_pyda` only special-cased the literal wildcard `_`;
  any other bare name fell through to "evaluate as an expression,
  compare via `__eq__`," failing on the undefined reference.

  Fix, mirroring the existing assignment-target/for-loop pattern
  exactly: `PY_case_block` is now split out of the generic-recurse
  bucket in `build_syms_pyda` — a bare, non-wildcard `PY_name` in
  pattern position gets `mark_store`'d (same helper assignment
  targets use), so it becomes a fresh `PYC_LOCAL` instead of an
  unresolved use. `build_match_pyda` gained a second branch
  (alongside the existing wildcard one): a bare-name pattern always
  matches (irrefutable) and, before running the case body, moves
  the subject's value into the newly bound local — the same shape
  as a plain `x = subject` assignment
  (`emit_assign_to_target`'s simple-name branch). Verified
  byte-identical to `python3` (`tests/match_capture.py` /
  `.exec.check`, executed and diffed, not just compile-checked, on
  both backends); full suite 180/0 (179 + the new test), no
  regressions.

  **Known caveat, NOT fixed here, pre-existing and general (not
  match/case-specific)**: pyc has no runtime check for reading an
  uninitialized local. `case 1: ...; case x: print(x)` correctly
  scopes `x` as function-local (matching Python's "assigned anywhere
  in the function ⇒ local everywhere" rule — confirmed the fixed
  capture pattern does NOT fall back to a same-named outer/global on
  the branches where it isn't itself taken), but reading `x` on a
  path where it was never actually assigned reads garbage stack
  memory instead of erroring (real Python raises
  `UnboundLocalError`). Reproduces identically with a plain
  `if cond: y = 5` / `print(y)` — nothing to do with `match`/`case`,
  a gap in pyc's whole local-variable model. Not filed as its own
  issue yet.
- **Or-patterns** (`case 1 | 2:`) — **broken, SILENT miscompile,
  no error at all**. Because `case_block: CASE_KW test ':' suite`
  parses the pattern as a generic expression, `1 | 2` parses as
  ordinary bitwise-OR and gets *evaluated* (to `3`) before being
  compared via `__eq__` — "match if subject equals 1 or 2" becomes
  "match if subject equals 3". Confirmed concretely:
  ```python
  def test_match(val):
      match val:
          case 1 | 2:
              print("one or two")
          case _:
              print("other")
  test_match(1); test_match(2); test_match(3)
  ```
  real Python: `one or two` / `one or two` / `other`. pyc: `other`
  / `other` / `one or two`. Compiles clean, runs clean, wrong
  answer — the dangerous kind of bug, since nothing signals it.
- **Guards** (`case x if x > 10:`) — **unparseable, hard syntax
  error**. `case_block`'s grammar rule has no optional `if test`
  clause at all. At least this fails loudly and immediately.
- **Class / sequence / mapping patterns** (`case Point(x=0, y=0):`,
  `case [a, b]:`, `case {"k": v}:`) — **never attempted**. Would
  hit the identical trap as or-patterns: `Point(x=0, y=0)` parses
  as an ordinary constructor call, constructs a real `Point`
  instance, and compares it via `__eq__` against the subject —
  wrong semantics, not just unimplemented, and just as silent as
  the or-pattern bug.

## Effort estimate per piece

Sizing based on the codebase's own precedent for similar binding /
lowering additions (assignment-target destructuring in issue 025,
the wildcard case already landed):

- **Capture patterns — DONE** (was estimated small/one-sitting;
  held, see fix description above).
- **Or-patterns — small-medium.** `build_match_pyda` needs to
  recognize a `PY_binop` with `|` in pattern position and chain
  multiple `__eq__` checks (`subject == 1 or subject == 2`) instead
  of evaluating the OR as a value. Contained to the same function;
  the complexity is walking/flattening a `1 | 2 | 3`-shaped pattern
  tree, not new machinery.
- **Guards — small.** Grammar: add an optional `if test` to
  `case_block`. Lowering: AND the guard's condition into the
  generated `if1_if` condition alongside the pattern match.
- **Class / sequence / mapping patterns — large, a small compiler
  feature in its own right** (matches the original filing's own
  framing). Needs real destructuring and attribute binding, not an
  equality check — comparable in scope to (likely larger than)
  issue 025's tuple-unpacking work.

With capture patterns done, guards + or-patterns together are
roughly what's left of the day's-focused-work estimate, and would
take `match`/`case` from "silently wrong for some common cases" to
"correct for everything except class patterns." Class patterns
remain a genuinely separate, large undertaking.

## Verification plan

1. ~~match/case: minimal literal+capture pattern test~~ — **done**:
   `tests/match_capture.py` / `.exec.check`, executed (not just
   compile-checked) and diffed against real `python3` output on
   both backends.
2. For each remaining pattern kind fixed, add a test file that
   **executes** and is checked against real Python's output
   (`.exec.check` or equivalent), not just a compile-only test — the
   or-pattern bug above is exactly the kind of thing a compile-only
   test would have missed, and this doc only found it by diffing
   runtime output against `python3`.
3. Once guards land, verify guard + capture interact correctly
   (`case x if x > 10:` — `x` must be bound before the guard
   condition evaluates).

## What this unblocks

Correct (not just compiling) `match`/`case` for the common literal
+ capture + guard + or-pattern subset — real Python code using
`match` today either fails to compile (capture patterns) or, worse,
silently produces wrong answers (or-patterns, and any future class
pattern use). Class/sequence/mapping pattern support is the
remaining large piece for full PEP 634 coverage.
