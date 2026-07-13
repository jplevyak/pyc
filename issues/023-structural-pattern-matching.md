# Issue 023: Structural pattern matching (`match`/`case`, PEP 634)

**Status:** open, one known limitation remains. **Capture, or-,
guard, sequence, mapping, and class patterns, plus `None`/`True`/
`False` singleton patterns and mixed-literal-type patterns, all
fixed 2026-07-12.** The one thing NOT fixed: `case None:` combined
with almost any other pattern in the same match statement hits a
deeper FA/codegen clone-generation gap (crashes at runtime) that
looks to be outside this file's lowering code entirely --
`build_match_pyda` refuses that specific combination at compile
time rather than risk shipping it. See "Known limitation" below.
**Affects:** `python.g` (grammar unchanged for this round --
class/mapping patterns parse via the existing constructor-call/
dict-literal grammar, same trick sequence patterns used with
list/tuple literals), `python_ifa_build_syms.cc` (`PY_case_block`
symbol building, `mark_pattern_captures`), `python_ifa_build_if1.cc`
(`build_match_pyda`, `build_pattern_match`, lowering),
`__pyc__/00_runtime.py` (unchanged in the end -- see the None
section for why an attempted addition there was reverted).

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
- **Mapping patterns** (`case {"k": v, ...}:`) — **FIXED
  2026-07-12**. Previously unimplemented: parsed as an ordinary dict
  literal (via python.g's existing `dictorsetmaker`/`PY_dict` — no
  grammar change needed) and compared via `__eq__` against the
  subject. `build_pattern_match`'s new `PY_dict` branch: keys are
  ordinary VALUE expressions (evaluated once, unconditionally --
  PEP 634 restricts mapping-pattern keys to literals/value patterns,
  never captures, so `mark_pattern_captures` leaves the key side
  untouched); `isinstance(subject, dict)`, then (nested inside that,
  same `guarded_bool` shape as sequence patterns) every key's
  presence checked via `__contains__`, then (nested again) each
  value retrieved via `__getitem__` and recursively matched.
  `**rest` (PEP 634's rest-of-mapping capture) isn't supported --
  python.g's `dictorsetmaker` has no `'**' NAME` alternative at all
  (real Python's dict-merge literal, `{**other}`, isn't a pyc
  feature either), so it fails to parse with an ordinary syntax
  error, same "doesn't parse, so at least loud" deferral as sequence
  patterns' `*rest`. Verified byte-identical to `python3` on both
  backends: flat mapping patterns with 1-3 keys, `case {}:` matching
  any mapping (including non-empty ones), and a guard on a mapping
  pattern (`tests/match_map.py` / `.exec.check`).
- **Class patterns** (`case Point(x=0, y=0):`) — **FIXED
  2026-07-12, keyword-only**. Previously unimplemented: parsed as an
  ordinary constructor call (via python.g's existing
  `power`/`trailer`/`arglist` grammar — no grammar change needed),
  constructed a REAL `Point` instance, and compared it via `__eq__`
  against the subject — wrong semantics, not just unimplemented, and
  just as silent as the or-pattern bug was (would have called
  `__init__` with the WRONG argument count/types too, for anything
  but a suspiciously-matching constructor signature).

  `build_pattern_match`'s new `PY_power`-with-`PY_call` branch
  recognizes this shape BEFORE it reaches the literal-pattern
  fallback (the same interception point that makes or-patterns and
  sequence patterns correct instead of silently wrong):
  `isinstance(subject, ClassName)`, then (nested in that branch) each
  keyword argument's name read off the subject via the exact `.attr`
  send `PY_power`'s own attribute-trailer handling emits
  (`build_attribute_get`, synthesized directly since there's no
  source-text `PY_attribute` node to hang it off), recursively
  matched against its sub-pattern. A bare dotted-name pattern with NO
  call trailer (`case Color.RED:`, a PEP 634 *value* pattern) is
  correctly NOT caught by this branch (it requires a `PY_call`
  trailer specifically) and falls through to the literal fallback
  unchanged, comparing via `__eq__` as before -- exactly the existing
  correct behavior for value patterns, undisturbed.

  **Positional class patterns are NOT supported**
  (`case Point(0, 0):`, matched via a `__match_args__` class
  attribute) -- fails loudly at compile time with a message pointing
  at the keyword-only form, rather than guessing. Real PEP 634
  attribute-name-to-position mapping needs a compile-time read-back
  of a class-body literal assignment (`__match_args__ = ("x", "y")`)
  that pyc has no existing machinery for (unlike sequence patterns,
  which could reuse `__getitem__`/`__len__` already built for plain
  unpacking) -- deferred as a separate, larger piece. Keyword-only
  class patterns (`Point(x=0, y=0)`) are PEP 634's more common,
  more explicit form regardless.

  Verified byte-identical to `python3` on both backends: multiple
  keyword attributes, nested class patterns (a class pattern as
  another class pattern's attribute value, and as a mapping
  pattern's value, and as a sequence pattern's element), a guard on
  a class pattern, and a polymorphic fallback arm
  (`tests/match_class.py` / `.exec.check`, plus broader combination
  coverage exercised manually during development -- see "What this
  unblocks" for the combined scratch repro).
- **`None`/`True`/`False` singleton patterns** (`case None:`,
  `case True:`) — **FIXED 2026-07-12, with one combination
  excluded**. These parse as bare `PY_name` nodes (this grammar has
  no keyword tokens for them -- they're ordinary global constants),
  which means they were being caught by the EXISTING capture-pattern
  branch (`case x:`'s handling, landed earlier in this issue) --
  `case None:` silently became an irrefutable capture binding a
  local literally named "None", matching UNCONDITIONALLY regardless
  of the subject's actual value. Confirmed concretely: a match with
  `case None: / case True: / case False: / case n:` printed "none"
  for every input (None, True, False, 5) before this fix, since the
  FIRST arm's capture always won. Same silent-miscompile bug class
  as or-patterns and (now) class/mapping patterns, just never
  exercised by earlier tests in this issue (all of which avoided
  `None`/`True`/`False` as *patterns*, though `None` obviously
  appears throughout pyc as an ordinary value elsewhere).

  Fix: `mark_pattern_captures` and `build_pattern_match`'s
  capture-pattern branches now explicitly exclude these three names
  (alongside the pre-existing `_` wildcard exclusion), routing them
  to new dedicated handling instead. `True`/`False`: NOT compared via
  `__eq__` against the raw `sym_true`/`sym_false` sentinel --
  confirmed empirically that fails FA type checking as a method
  ARGUMENT the same way `combine_bool`'s own comment already
  documents it failing as a method RECEIVER (a compile-time marker,
  not a properly-typed runtime bool instance). Instead: narrowed via
  `isinstance(subject, bool)`, then (nested in that branch) simply
  "is `subject` truthy" / "is it falsy" -- once narrowed to `bool`,
  identity and truthiness coincide, sidestepping the sentinel
  entirely. (A same-shaped `__pyc_is_bool__()` virtual-dispatch
  alternative was tried first, adding a new `object`/`bool`-overridden
  method mirroring `__null__`'s existing pattern -- reverted: it
  failed differently, confirmed via a minimal non-match/case repro
  that calling a BRAND NEW, non-primitive-registered method directly
  on an `int`-typed receiver fails outright, unrelated to match/case
  -- `pyc_symbols.h`'s `S(...)`/`P(...)` dunder registry looks
  load-bearing for which methods are dispatchable on "primitive"
  types, and adding to it was out of scope here.)

  `None`: see "Known limitation" below -- the identity check itself
  (`isinstance(subject, sym_nil_type)`, matching the pre-existing
  `x is None` expression lowering) works fine in isolation, but
  triggers a deeper bug when combined with most other patterns.

  Verified byte-identical to `python3` on both backends: `True`/
  `False` mixed with int/string fallback subjects
  (`tests/match_literal_types.py`); `None` combined with a wildcard
  fallback across None/int/string/list subjects
  (`tests/match_none.py`).
- **Mixed-literal-type patterns** (`case 5: ... case "hi": ...` in
  one match) — **FIXED 2026-07-12**, found while testing the above.
  The EXISTING literal-pattern fallback (predates this round of
  fixes) compared unconditionally via `__eq__`, which -- exactly
  like sequence/mapping/class patterns before their own
  `guarded_bool` fixes -- has to type-check against the subject's
  FULL static union across every OTHER arm in the same match. A
  match combining `case 5:` and `case "hi":` crashed the underlying
  C compiler (`comparison between pointer and integer` /
  `no matching function for call to _CG_str_eq`) once the subject
  was polymorphic enough to include both int- and string-typed
  arms. Fixed the same way: literal number/string patterns are now
  narrowed via `isinstance(subject, int/float/str)` before the
  `__eq__` comparison (int vs. float distinguished by the same
  '.'/'e'/'E'/'j' text scan `make_num_pyda` already uses to build
  the literal's own value, factored into `number_pattern_is_float`).
  Patterns this can't classify (dotted value patterns like
  `Color.RED`) keep the old unconditional form -- narrower coverage
  than before for that one sub-case, but no worse than the
  pre-existing behavior. Verified via `tests/match_literal_types.py`
  and the broader `tests/match_class.py`/`match_map.py` combination
  coverage.

## Known limitation: `case None:` combined with most other patterns

`case None:` may only be combined with a wildcard (`case _:`) and/or
other `case None:` arms in the same match statement. Combined with
ANYTHING else -- a capture (`case x:`), a literal, `True`/`False`,
or a sequence/mapping/class pattern -- compiled code crashes at
runtime with `Assertion '!"runtime error: matching function not
found"'`. `build_match_pyda` detects this combination (via
`pattern_contains_none` / `pattern_is_risky_with_none`, scanning
every case pattern in the match) and refuses to compile it, with a
message pointing at the workaround (split into a separate match
statement, or use a guard: `case x if x is None:`).

**Ruled out as the same bug as (closed)
[026-polymorphic-method-dispatch-partial-override-crash.md](closed/026-polymorphic-method-dispatch-partial-override-crash.md)**
(identical assertion text, `Assertion '!"runtime error: matching
function not found"'`) -- that was the leading hypothesis when both
issues were open at once, but 026 has since been root-caused and
fixed (a `Type_SUM`-typed receiver formal for a method reached only
through class inheritance, e.g. a subclass with no override of its
own). With that fix landed, this limitation was re-tested directly
(temporarily bypassing the compile-time refusal below) and STILL
crashes, identically, with `PYC_DBG_DISPATCH=1` showing the failure
is in `__str__` dispatch (from `print()` formatting the
capture-bound fallback variable) over a subject typed
`None | int | float` -- none of which carry a classtag
(`int`/`float`/`None` are primitive/system types, not
`Type_RECORD`), so this is dispatch over a union of PRIMITIVE/BOXED
types, a completely different mechanism from 026's class-method
classtag gap. Genuinely a separate, still-open problem -- see 026's
own "Ruled out" section for the cross-check.

This was investigated in real depth, not just noticed and shelved.
Confirmed:
- The crash is NOT specific to which mechanism `case None:`'s own
  check uses -- three different lowerings were tried (a
  `subject.__null__()` method dispatch; the ordinary-call
  `isinstance(subject, sym_nil_type)` form every other pattern kind
  here uses; the raw `prim_isinstance` primitive send, the EXACT
  form the pre-existing `x is None` expression lowering already uses
  successfully) -- all three fail identically once combined with
  another reachable case.
- The crash is NOT specific to isinstance-narrowed patterns
  specifically -- `case None: / case x:` (a plain CAPTURE fallback,
  no narrowing at all) crashes too, at the point `x`'s binding MOVE
  runs.
- The crash IS specific to whether the OTHER case does anything
  besides an unconditional no-op match -- `case None: / case _:`
  (wildcard, no binding) compiles and runs correctly regardless of
  how many distinct runtime types the subject spans.
- A plain (non-match/case) `if val is None: ... elif isinstance(val,
  int): ...` compiles and runs correctly -- so this isn't a general
  "None-typed union member" limitation in pyc, it's specific to
  something about the CHAIN of nested `if1_if`s `build_match_pyda`
  generates for a multi-arm match statement.

This points at a genuine codegen gap -- dispatching a method call
(here, `__str__`, needed by `print()`) on a subject whose type spans
several PRIMITIVE/boxed basic types (`None`/`int`/`float`/...), none
of which carry the classtag mechanism issue 026's classtag dispatch
relies on -- rather than anything fixable from this file's lowering
code. Worth a dedicated `ifa/issues/` entry if pursued further --
not filed as of this writing, since the compile-time refusal here
keeps it from being a correctness hazard in the meantime.

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
- **Mapping patterns — DONE** (was bundled with class patterns as
  "large, a small compiler feature in its own right" — held on
  size/shape: key-existence + value-recursion turned out to be a
  direct structural echo of sequence patterns' isinstance/length/
  element nesting, minus the length check, plus a per-key
  `__contains__` fold. `**rest` explicitly deferred, matching
  sequence patterns' `*rest` deferral).
- **Class patterns — DONE, keyword-only** (positional patterns via
  `__match_args__` deferred — see the fix description above for why:
  no existing compile-time class-body-literal read-back to build on,
  unlike sequence patterns' reuse of `__getitem__`/`__len__`).
  Comparable in scope to issue 025's tuple-unpacking work as
  estimated, though the actual mechanism (an isinstance check plus
  N independent attribute reads) turned out simpler than
  `__match_args__`-based positional matching would have been.
- **`None`/`True`/`False` singleton patterns — DONE, with the
  `None`-combination limitation described above.** Not originally
  called out as a separate estimate line (folded into "capture
  patterns" implicitly, since they're both bare-`PY_name` shapes) —
  in hindsight should have been: finding that `case None:` was
  silently swallowed by the capture-pattern branch, diagnosing why
  `True`/`False` can't compare against the raw `sym_true`/`sym_false`
  sentinel, and the None+isinstance-narrowing crash investigation
  together took longer than either mapping or class patterns.
- **Mixed-literal-type patterns — DONE**, not part of the original
  filing at all — a latent bug in the PRE-EXISTING literal-pattern
  fallback (predates this issue's 2026-07-12 round entirely), found
  only because testing class/mapping patterns required combining
  several literal types in one match statement for the first time.

With capture, or-patterns, guards, sequence, mapping, and class
patterns, plus singleton and mixed-literal-type patterns, all done,
`match`/`case` covers all of PEP 634 except positional class
patterns (`Point(0, 0)` via `__match_args__`) and the two
star-capture forms (`case [a, *rest]:`, `case {**rest}:`) — three
narrow, individually-scoped, explicitly-deferred gaps — plus the
one `None`-combination runtime limitation documented above.

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
5. ~~Mapping-pattern test, executed and diffed~~ — **done**:
   `tests/match_map.py` / `.exec.check` (1-3 key patterns, `case {}:`,
   a guard on a mapping pattern, polymorphic fallback).
6. ~~Class-pattern test, executed and diffed~~ — **done**:
   `tests/match_class.py` / `.exec.check` (multiple keyword
   attributes, a guard on a class pattern, polymorphic fallback;
   nesting with mapping/sequence/class patterns exercised manually
   during development, see the class-pattern fix description above).
7. ~~Singleton (`None`/`True`/`False`) and mixed-literal-type test,
   executed and diffed~~ — **done**: `tests/match_literal_types.py`
   (`True`/`False` mixed with int/string/list subjects, plus
   int/float/string literal patterns all in one match) and
   `tests/match_none.py` (`None` combined with a wildcard fallback
   across None/int/string/list subjects — the one combination
   confirmed safe).

## What this unblocks

Correct (not just compiling) `match`/`case` for literal + capture +
or-pattern + guard + sequence + mapping + class patterns, plus
singleton (`None`/`True`/`False`) patterns and mixed-literal-type
matches, is now landed — real Python code using these forms compiles
and runs correctly on both backends. What remains: positional class
patterns, the two star-capture forms (all three narrow and
explicitly deferred, not silent traps -- they fail to parse or fail
loudly at compile time), and the `case None:`-combination runtime
limitation (also a loud compile-time refusal, not a silent trap).
None of PEP 634's pattern KINDS are unimplemented anymore; what's
left is refinement within kinds already landed.
