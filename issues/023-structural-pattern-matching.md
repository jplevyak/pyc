# Issue 023: Structural pattern matching (`match`/`case`, PEP 634)

**Status:** open, partial. **Capture patterns, or-patterns, guards,
and sequence patterns fixed 2026-07-12**; class/mapping patterns
remain.
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
- **Or-patterns** (`case 1 | 2:`) — **FIXED 2026-07-12**. Was a
  silent miscompile, no error at all: because
  `case_block: CASE_KW test ':' suite` parses the pattern as a
  generic expression, `1 | 2` parsed as ordinary bitwise-OR and got
  *evaluated* (to `3`) before being compared via `__eq__` — "match
  if subject equals 1 or 2" silently became "match if subject
  equals 3." Confirmed concretely before the fix: for
  ```python
  def test_match(val):
      match val:
          case 1 | 2:
              print("one or two")
          case _:
              print("other")
  test_match(1); test_match(2); test_match(3)
  ```
  real Python printed `one or two` / `one or two` / `other`; pyc
  printed `other` / `other` / `one or two`. Compiled clean, ran
  clean, wrong answer — the dangerous kind of bug, since nothing
  signals it.

  Fix (`build_match_pyda`, `python_ifa_build_if1.cc`): a new
  `flatten_or_pattern` helper walks the left-folded
  `PY_binop('|')` tree `python.g`'s `build_binop_list` produces for
  `1 | 2 | 3` (`((1 | 2) | 3)`) into its ordered leaf patterns.
  `build_match_pyda` gained a third branch (alongside wildcard and
  capture): evaluate every alternative's `__eq__` check
  unconditionally (no side effects to short-circuit around — these
  are literal comparisons) and combine the per-alternative booleans
  with `bool.__or__` into one combined result, then a single
  `if1_if` on that combined boolean. (First attempt nested one
  `if1_if` per alternative, all pointing at the same `case_then`
  `Code*` — hit `if1_flatten_code`'s "already flattened" assert;
  IF1 code trees require each node to be reachable exactly once, so
  `case_then` has to be referenced by exactly one `if1_if`, same as
  every other branch in this dispatch. The single-combined-boolean
  restructure fixed it.) A bare name as an or-pattern alternative
  (`case x | 1:`) fails loudly at compile time (`fail(...)`) rather
  than silently doing the wrong thing — PEP 634 allows this in
  principle (the capture binds from whichever alternative matched)
  but it needs its own design (which alternative's capture wins,
  and pyc's per-case single-pass lowering doesn't have an obvious
  place to thread that), so it's explicitly out of scope here rather
  than guessed at.

  Verified byte-identical to `python3` for 2-way, 3-way, and
  mixed or-pattern/capture-pattern match statements
  (`tests/match_or.py` / `.exec.check`, executed and diffed on both
  backends). Full suite 181/0 (180 + the new test), no regressions.
- **Guards** (`case x if x > 10:`) — **FIXED 2026-07-12**. Was
  unparseable (`case_block`'s grammar rule had no optional
  `if test` clause at all — a hard syntax error, at least a loud
  failure rather than a silent one).

  Grammar (`python.g`): added a named `case_guard: 'if' test`
  sub-rule (mirroring `elif_clause`/`else_clause`'s existing
  "named clause" convention) and made it optional in `case_block`:
  `CASE_KW test case_guard? ':' suite`. New `PY_case_guard`
  `PyASTKind` (`python_ast.h`).

  Lowering (`build_match_pyda`): a guard is located by scanning
  `case_block`'s children for a `PY_case_guard` kind
  (`find_case_guard`) rather than by position — this codebase's
  existing `case_block` child-indexing is already order-fragile
  (the pre-existing `children.n == 3` / `>= 4` / else dance for
  locating the pattern and suite), and a guard shifts child counts
  around further; scanning by kind sidesteps needing to reason about
  every combination. A new `eval_case_guard` helper evaluates the
  guard's condition into a boolean Sym (or returns `nullptr` if
  there's no guard, so every call site can treat "no guard" and
  "guard present" uniformly). Threaded through all four pattern
  branches:
  - wildcard/capture (previously unconditional matches): if a guard
    is present, wrap `case_then` in an `if1_if` gated on just the
    guard (the pattern itself always matches, so the guard alone
    decides) instead of the previous unconditional `if1_gen`.
  - literal/or-pattern (already compute a match boolean): AND the
    guard's boolean into the existing match boolean via
    `bool.__and__` before the `if1_if`.

  For the capture case specifically, the guard is evaluated *after*
  the capture's bind — `case x if x > 10:` needs `x` bound before
  the guard can reference it, and `build_syms_pyda`'s existing
  generic-recurse over `case_block`'s children (which visits the
  pattern, marked `PY_STORE`, before the guard) already gets the
  symbol ordering right without needing its own guard-specific
  handling.

  Verified byte-identical to `python3` across guards combined with
  every other pattern kind in one match statement — literal,
  or-pattern, capture, and `case _ if cond:` — on both backends
  (`tests/match_guard.py` / `.exec.check`). Full suite 182/0 (181 +
  the new test), no regressions.
- **Sequence patterns** (`case [a, b]:`, `case (1, x):`) —
  **FIXED 2026-07-12**. Previously unimplemented: `[a, b]` parsed
  as an ordinary list-literal expression and got compared via
  `__eq__` against the subject — wrong semantics (a list literal
  containing unbound names `a`/`b` would have failed to even
  compile), same trap class as the or-pattern and class/mapping
  bugs.

  This landed alongside a structural rewrite of the matcher: the
  previous `build_match_pyda` had one flat, non-recursive dispatch
  (wildcard / capture / or-pattern / literal, each a top-level `if`
  in the function) with a matching flat boolean-AND-fold for
  combining sub-results. Sequence patterns need real recursion —
  each element is itself an arbitrary pattern, including another
  sequence pattern — so the dispatch moved into a new
  `build_pattern_match(pattern, subject, code, case_ast, ctx,
  guard_eval=nullptr) -> Sym*` that calls itself for: or-pattern
  alternatives, and (new) sequence-pattern elements.
  `build_match_pyda` is now a thin per-case-block driver that calls
  `build_pattern_match` once per case and wires the returned
  boolean into an `if1_if`/chain, same shape as before.

  Sequence-pattern matching itself
  (`build_pattern_match`'s `PY_list`/`PY_tuple` branch): `isinstance(subject,
  list) or isinstance(subject, tuple)` (PEP 634 explicitly excludes
  str/bytes even though they support `__len__`/`__getitem__` too —
  `case [a, b]:` must not match `"hi"`), then `len(subject) ==
  n_elts`, then a recursive `build_pattern_match` call per element
  against `subject[j]`, AND-folded together. Star patterns
  (`case [a, *rest]:`) are not supported yet — same underlying
  grammar gap as issue 024's extended-unpacking assignment targets
  — deferred as a follow-on.

  Symbol-table side (`python_ifa_build_syms.cc`): a new
  `mark_pattern_captures` walks the SAME pattern-tree shape
  `build_pattern_match` recurses over (wildcard exclusion,
  or-pattern alternatives, sequence-pattern elements) and
  `mark_store`s every bare-name capture it finds, replacing the
  existing (capture-only) handling in `PY_case_block`'s
  `build_syms_pyda` case. Needed its own function rather than
  reusing `mark_store` directly because `mark_store` doesn't
  recurse into `PY_list` (only `PY_tuple`/`testlist`/`exprlist`/
  `fpdef`/`fplist`), so `case [a, b]:`'s list-syntax elements
  wouldn't have gotten bound by the existing helper.

  **Two real bugs found and fixed while landing this, both
  general lessons beyond match/case:**

  1. **FA type-checks the whole program statically, not per-runtime-
     branch.** The first working version emitted
     `subject.__len__()`/`subject.__getitem__(j)`
     *unconditionally* (only their boolean *results* were
     AND-folded into the overall match boolean) — this compiled
     and ran correctly for a subject that was always a list, but a
     polymorphic call site (`match val: case [a, b]: ... case x:
     ...` called with both a list and an int) failed with
     "expression has no type": FA has to type-check `__len__`/
     `__getitem__` against `subject`'s *full static union type*
     (`list | int`) across the whole program, not just within the
     runtime branch where the type actually matched, and `int`
     doesn't implement those methods. Fixed with a new
     `guarded_bool(cond, code, case_ast, build_then)` helper that
     builds *genuine nested* `if1_if` control flow (as opposed to a
     flat boolean AND-fold) so FA's existing isinstance-narrowing
     logic can narrow `subject`'s static type *within* the "then"
     branch before type-checking the length/element-access calls
     against it. Sequence-pattern matching now nests two levels of
     `guarded_bool`: outer gated on the isinstance check, inner
     gated on the length check, elements matched (recursively)
     inside the inner one.
  2. **Guards evaluated outside a pattern's own nested control flow
     can read bindings that only conditionally exist.** Found via
     `case [a, b] if a > b:`: the guard's `a > b` failed to
     type-check (`unresolved call '__gt__'`, "'a' has no type")
     because the previous design evaluated every case's guard
     *after* `build_pattern_match` returned, in the flat outer code
     stream — but `a`/`b`'s bindings now only happen *inside* the
     nested `guarded_bool` "then" branches added for bug 1 above
     (only reached once isinstance+length checks already passed).
     Reading them from outside that nesting is exactly the
     "reference a binding FA can't prove is live on this path"
     problem, just surfacing as a hard type error here instead of
     silently reading garbage (contrast with issue 039's
     uninitialized-read case, where the same class of gap produces
     UB instead of a diagnostic). Fixed by threading the guard down
     *into* `build_pattern_match` as an optional callback
     (`guard_eval`), invoked by each pattern kind at the exact point
     its own match — and any bindings it made — are established:
     for capture/literal/or-patterns (flat, unconditional bindings)
     this coincides with "right after `build_pattern_match`
     returns," same as before; for sequence patterns it's now
     *inside* the innermost `guarded_bool` "then" branch, evaluated
     exactly once, only along the path where the pattern already
     matched — which also fixed a latent double-evaluation-adjacent
     correctness issue: the guard is now only evaluated when the
     pattern structurally matched, matching real Python's short-
     circuit semantics, rather than always running regardless of
     match outcome.

  Verified byte-identical to `python3`, on both backends, for: flat
  sequence patterns (`case [a, b]:`, `case [a, b, c]:`, `case []:`)
  mixed with a polymorphic capture fallback (list/int/str subjects
  — confirming str is correctly excluded); tuple-literal pattern
  syntax (`case (1, x):`); nested sequence patterns (`case [[a, b],
  c]:`); or-pattern as a sequence-pattern element (`case [1 | 2,
  y]:`); and a guard on a sequence pattern (`case [a, b] if a >
  b:`) — all combined in one match statement
  (`tests/match_seq.py` / `.exec.check`). Full suite 183/0 (182 +
  the new test) on both the C and LLVM backends, no regressions.
- **Class / mapping patterns** (`case Point(x=0, y=0):`,
  `case {"k": v}:`) — **never attempted**. Class patterns would hit
  the identical trap sequence/or-patterns did: `Point(x=0, y=0)`
  parses as an ordinary constructor call, constructs a real `Point`
  instance, and compares it via `__eq__` against the subject —
  wrong semantics, not just unimplemented, and just as silent as
  the or-pattern bug was.

## Effort estimate per piece

Sizing based on the codebase's own precedent for similar binding /
lowering additions (assignment-target destructuring in issue 025,
the wildcard case already landed):

- **Capture patterns — DONE** (was estimated small/one-sitting;
  held, see fix description above).
- **Or-patterns — DONE** (was estimated small-medium; held —
  contained entirely to `build_match_pyda`, the actual complexity
  was the `Code*`-sharing assert rather than the pattern-flattening
  itself, see fix description above).
- **Guards — DONE** (was estimated small; held — the grammar
  addition was as small as expected, the lowering ended up
  touching all four pattern branches rather than one shared spot,
  but each touch was mechanical, see fix description above).
- **Sequence patterns — DONE** (was estimated large, bundled with
  class/mapping patterns as "a small compiler feature in its own
  right" — held on size: the recursive-matcher refactor plus the
  isinstance/length/element-binding lowering was the bulk of it, but
  the two bugs found while landing it (FA's whole-program static
  typing needing genuine nested control flow to narrow within a
  branch; guards needing to be evaluated inside that same nesting
  rather than after it) took as much time as the feature itself.
  Star patterns (`case [a, *rest]:`) explicitly deferred, matching
  issue 024's extended-unpacking gap).
- **Class / mapping patterns — large, a small compiler feature in
  its own right** (matches the original filing's own framing).
  Mapping patterns need key-existence + value-recursion (structurally
  similar to what sequence patterns just landed, minus the
  length-equality check, plus a `**rest` capture form). Class
  patterns need real attribute binding (`Point(x=0, y=0)`'s
  positional/keyword sub-patterns matched against `__match_args__`/
  named attributes) — comparable in scope to (likely larger than)
  issue 025's tuple-unpacking work, and is the one PEP 634 pattern
  kind pyc has no adjacent precedent for at all (no existing
  attribute-destructuring lowering to crib from, unlike sequence
  patterns which could reuse `__getitem__`/iteration machinery
  already built for plain unpacking).

With capture, or-patterns, guards, and sequence patterns done,
class/mapping patterns are the only piece left, taking `match`/
`case` from "correct for everything except class/mapping patterns"
(now) to full PEP 634 coverage.

## Verification plan

1. ~~match/case: minimal literal+capture pattern test~~ — **done**:
   `tests/match_capture.py` / `.exec.check`, executed (not just
   compile-checked) and diffed against real `python3` output on
   both backends.
2. ~~Or-pattern test, executed and diffed~~ — **done**:
   `tests/match_or.py` / `.exec.check` (2-way, 3-way, and
   or-pattern/capture-pattern mixed in one match statement).
3. ~~Guard test, executed and diffed~~ — **done**:
   `tests/match_guard.py` / `.exec.check` (guards combined with
   literal, or-pattern, capture, and wildcard patterns, including
   `case x if x > 10:` verifying the capture-before-guard binding
   order).
4. ~~Sequence-pattern test, executed and diffed~~ — **done**:
   `tests/match_seq.py` / `.exec.check` (flat sequence patterns with
   capture/wildcard/literal elements against polymorphic list/int/str
   subjects; tuple-literal syntax; nested sequence patterns;
   or-pattern-as-element; guard-on-sequence-pattern — all on both
   backends).

## What this unblocks

Correct (not just compiling) `match`/`case` for literal + capture +
or-pattern + guard + sequence-pattern is now landed — real Python
code using these forms compiles and runs correctly on both
backends. Class/mapping pattern support (currently unimplemented,
would hit the same silent-miscompile trap or-patterns/sequence
patterns did before their fixes) is the one remaining piece for
full PEP 634 coverage.
