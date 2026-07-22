# Issue 023: Structural pattern matching (`match`/`case`, PEP 634)

**Status:** RESOLVED 2026-07-21 — every PEP 634 pattern KIND,
INCLUDING all three "rest capture" forms (`*rest` in sequence
patterns, `**rest` in mapping patterns, positional class patterns via
`__match_args__`), is implemented and verified byte-identical to real
`python3` output on both backends (base landed 2026-07-12; positional
class patterns, sequence-pattern star capture, and mapping-pattern
`**rest` all added 2026-07-21; re-verified same day). The last
remaining limitation — `case None:` combined with a narrowing or
capturing pattern — is fixed: mechanism 1 in `build_isinstance_call`
and mechanism 2 (the general `None`-plus-scalar contour merge) in
`type_cannonicalize`, both landed 2026-07-21 under
[ifa/issues/060](../ifa/issues/060-none-branch-dropped-mixed-with-literal-bool-sequence.md),
and the compile-time guard has been removed. `tests/match_none.py`
covers all four previously-blocked combinations. No pattern-matching
features or limitations remain.
**Affects:** `python.g` (`match_stmt`/`case_block`/`case_guard`
grammar, soft-keyword `match`/`case`; `listmaker`/`testlist_comp` now
accept `testlist_item` -- issue 024's `star_expr`/`PY_star_expr` --
instead of a bare `test`; `dictorsetmaker` now accepts a trailing
`dict_rest_arg` -- a `PY_dstar_arg`-producing rule dedicated to this
use, not the existing `dstar_arg` used by `**kwargs` parameters,
whose optional type-annotation clause doesn't belong here),
`python_ifa_build_syms.cc` (`mark_pattern_captures`,
`collect_match_args`, `gen_class_pyda`), `python_ifa_build_if1.cc`
(`build_match_pyda`, `build_pattern_match`,
`PycCompiler::building_assign_target`), `ifa/if1/sym.h`
(`Sym::match_args`).

## What's implemented

Literal, wildcard, capture, or-, guard, sequence, mapping, and class
(keyword AND positional) patterns, plus `None`/`True`/`False`
singleton patterns and mixed-literal-type matches. Several of these
were originally **silent miscompiles**, not just missing features —
`case 1 | 2:` evaluated as ordinary bitwise-OR before comparing;
`case None:` was swallowed by the capture-pattern branch and matched
*unconditionally*; `case Point(x=0):` constructed a real `Point` and
compared it via `__eq__` — worth knowing if auditing old pyc output,
not something to act on now.

**Positional class patterns** (`case Point(0, 0):`, added 2026-07-21):
resolved against `cls->match_args`, a new `Sym` field populated by
`collect_match_args` — a compile-time, syntax-only read-back of the
class body's own `__match_args__ = ("x", "y")` literal (falling back
to the nearest base class's `match_args` if the matched class
declares none itself, mirroring Python's normal attribute lookup).
Positional args resolve to attribute names and are merged with any
keyword args in the same call into the exact same isinstance +
attribute-read + recursive-match machinery the keyword form already
used — no FA/codegen changes needed. Mixing positional and keyword
args in one pattern (`case Point(0, y=5):`), and a subclass inheriting
its base's `__match_args__`, both work. A positional arg with no
corresponding `__match_args__` entry, a positional arg after a keyword
arg, and a positional/keyword pair naming the same attribute twice all
fail loudly at compile time (matching CPython's real `TypeError`/
`SyntaxError` wording where practical) rather than guessing.
`pattern_contains_none` (the `case None:`-combination safety net
below) also now recurses into positional class-pattern args, not just
keyword ones -- checked empirically that a nested `None` in a
positional arg (`case Point(None, y):`) doesn't actually reproduce the
runtime crash (the nested attribute value is a different Sym than the
outer match subject), but the keyword form was already conservatively
refused for the same shape, so the positional form now matches it
rather than leaving an inconsistent safety net between the two.
`mark_pattern_captures` (`python_ifa_build_syms.cc`) needed the same
treatment for a different reason -- caught before this landed, not a
shipped bug: its class-pattern branch only recursed into keyword arg
VALUES, so a bare positional capture (`case Point(x, y):`) never got
marked `PY_STORE` and silently resolved to (and mutated) a same-named
OUTER variable instead of binding a fresh local, confirmed via a
shadowing test (`x = 999` at module scope, then `case Point(x, y):`
inside a function) that diverged from `python3`'s output before the
fix. Now mirrors `build_pattern_match`'s own arg loop, recursing into
positional args exactly like keyword values. Covered by
`match_class_positional.py`'s last test.

**Star capture in sequence patterns** (`case [a, *rest]:`, added
2026-07-21): `python.g`'s `listmaker`/`testlist_comp` (the grammar
`[...]`/`(...)` list/tuple LITERALS use -- match/case patterns ride
this same grammar, no dedicated pattern grammar exists) now accept
`testlist_item` (`test | star_expr`) per element instead of a bare
`test`, reusing issue 024's `star_expr`/`PY_star_expr` node wholesale.
`build_pattern_match`'s sequence-pattern branch: at most one star
element (else `fail()`, matching CPython's real "multiple starred
names in sequence pattern" wording -- confirmed via `compile()` that
real Python defers this to a semantic check, not the grammar, same as
implemented here); its target must be a plain name or `_` (else
`fail()` -- confirmed via `ast.parse` that CPython's OWN grammar
restricts it the same way, so there's no recursive sub-pattern to
match against a star capture, just bind-or-discard). Leading/trailing
elements bind positionally as before; the length check relaxes from
`==` to `>=`; the star target binds a NEW list built via a runtime
loop over the middle range -- mirrors `emit_assign_to_target`'s
existing star-target loop for ordinary assignment (issue 024)
exactly, `idx = n_leading; while idx < limit: rest.append(subject[idx]);
idx += 1`. No FA/codegen changes.

Two related issues caught and fixed before/while landing this:

- Since `listmaker`/`testlist_comp` are also the grammar for
  ORDINARY list/tuple literal expressions (not just patterns), the
  change also newly parses `*y` inside a plain `[1, *y, 2]`/
  `(1, *y, 2)` (PEP 448 literal unpacking) where it used to be a
  clean syntax error. pyc doesn't implement real PEP 448 semantics,
  so `build_if1_pyda`'s ordinary (non-pattern) `PY_list`/`PY_tuple`
  case now explicitly `fail()`s on a `PY_star_expr` child, keeping
  that gap loud instead of letting it silently build `*y` as a single
  unspread element (its own `PY_star_expr` case just forwards the
  inner value unchanged).
- That defensive check initially broke `tests/star_unpack.py`
  (issue 024's OWN test): `build_if1_assign_target` pre-builds an
  assignment target tree via the SAME generic `build_if1_pyda`
  dispatch (to populate per-node `->code`/`->rval`/`->sym`/
  `->is_member` for `emit_assign_to_target` to read back) -- so
  `a, *b = [1, 2, 3]`'s target, ALSO a `PY_tuple` node with a
  `PY_star_expr` child, hit the same new check. Fixed with a small
  `PycCompiler::building_assign_target` flag, set around that one
  pre-build call, that the defensive check skips.
- `mark_pattern_captures` needed a new `PY_star_expr` case (recursing
  into the inner name, mirroring `mark_store`'s existing handling for
  the assignment-target case) -- without it, `rest` would resolve as
  an ordinary read of (and the match would silently mutate) any
  same-named OUTER variable instead of binding a fresh local, the
  exact same class of bug caught for positional class-pattern
  captures above. Caught the same way: a shadowing test, covered as a
  permanent regression test in `match_seq_star.py`.

**`**rest` in mapping patterns** (`case {"k": v, **rest}:`, added
2026-07-21): `python.g`'s `dictorsetmaker` (the `{...}` dict/set-
LITERAL grammar -- match/case patterns ride this same grammar) gets
a new `dict_rest_arg: '**' NAME` rule, collected as one trailing
child after the existing flat key/value pairs -- an ODD total child
count signals "last child is the rest capture" to every consumer
(mirrors how `new_pyast_collect`'s flat DFS already produces
alternating key/value pairs for the plain form). Real Python
structurally restricts `**rest` to at most one, and only last
(confirmed via `ast.parse`: `{**a, "k":1}` and `{**a, **b}` are both
grammar-level `SyntaxError`s, not semantic checks) -- pyc's grammar
enforces the same restriction for free, unlike sequence patterns'
star (whose more permissive host grammar needed an explicit "at most
one" runtime check). One thing pyc's grammar does NOT structurally
prevent that real Python's does: `**_` (a wildcard rest, illegal in
real Python -- confirmed via `ast.parse`, unlike sequence patterns'
`*_`) -- `build_pattern_match` explicitly `fail()`s on it.
`build_pattern_match`'s mapping-pattern branch: the existing
per-key `__contains__`/`__getitem__`/recursive-match logic for the
explicit keys is unchanged; when `**rest` is present, builds a NEW
`dict()` (a real constructor call, like the existing dict/set literal
building -- unlike list/tuple's low-level `sym_make` primitive),
iterates `subject`'s keys via its own `__iter__`/`__pyc_more__`/
`__next__` protocol, and copies every key not already claimed by an
explicit pattern (checked against a small `excluded` list of the
already-evaluated pattern keys via `list.__contains__`, issue 008)
into the new dict -- always a plain `dict`, even from a `dict`
SUBCLASS subject, confirmed against real CPython, mirroring sequence
patterns' "*rest is always a plain list" rule. The built dict binds
to the `**rest` name by reusing `build_pattern_match`'s own bare-name
capture-pattern branch directly, same trick the sequence-pattern star
capture uses. No FA/codegen changes.

Three related issues caught and fixed while landing this, the first
two mirroring the star-capture landing's own findings exactly:

- `dictorsetmaker` is also the grammar for ORDINARY dict literal
  expressions, so this also newly parses `**other` inside a plain
  `{"k": 1, **other}` (PEP 448 dict-merge) where it used to be a
  clean syntax error (an ordinary dict literal with `**other` FIRST
  or in the MIDDLE, e.g. `{**other, "k": 1}`, still doesn't parse --
  a side effect of the grammar's own "rest must be last" shape, still
  loud either way). `build_if1_pyda`'s ordinary (non-pattern)
  `PY_dict` case now explicitly `fail()`s on a trailing
  `PY_dstar_arg` (careful to exclude the dict-COMPREHENSION shape,
  `{k: v for ...}`, which is ALSO a 3-child/odd-count AST but
  unrelated) -- the flat-literal loop below only walks children in
  pairs, so a trailing rest arg would otherwise be silently DROPPED
  (an observably-wrong dict missing the merge), not just misread.
- `mark_pattern_captures` needed a new case for the trailing
  `**rest` child, same shadowing-bug shape as the sequence-pattern
  star capture and the positional-class-pattern captures -- caught
  the same way, via a shadowing test, before it shipped.
- Unlike the star-capture landing, no `PycCompiler::building_assign_target`-
  style collision existed to fix here: a dict literal is never a
  legal assignment target in Python, so there's no analogous
  "grammar shared with a target-tree prebuild pass" risk.

**Found, not fixed -- a pre-existing dead-code residual, not a new
logic bug:** compiling two SEPARATE functions that each `case
{"key": x, **rest}:` against the SAME literal key, where one
function's captured `x` goes unused in its body, produces a clang
`-Wunused-value` warning in the generated C (a computed-but-discarded
`dict._vals[i]` index expression, immediately followed by a hardcoded
`return 0;`) -- confirmed via the generated C that this is a
provably-dead residual from FA/codegen determining a SHARED
`dict.__getitem__` clone's return value unneeded by (at least) one of
its call sites, the same general shape issue 011 already documented
("one harmless dead assignment left over," `mark_live_code` computed
before a later fold) and explicitly left as a low-value residual
rather than fixing. Confirmed NOT a correctness bug: every
combination tried (many) produced byte-identical output to real
`python3` regardless of whether the warning fired. `tests/match_map_star.py`
avoids the trigger (all captured names are used) rather than
papering over it with an accepted-output sidecar; not filed as its
own issue since it's a duplicate of 011's already-accepted class, not
a new one.

Test coverage (all passing, both backends): `tests/match_basic.py`,
`match_capture.py`, `match_or.py`, `match_guard.py`, `match_seq.py`,
`match_seq_star.py`, `match_map.py`, `match_map_star.py`,
`match_class.py`, `match_class_positional.py`, `match_literal_types.py`,
`match_none.py`.

One lesson from this work worth remembering beyond match/case: FA
type-checks a call against a subject's *whole static union type*
across the entire program, not just within the runtime branch where
the type actually narrowed. Sequence/mapping/class pattern matching
(and the pre-existing literal-pattern fallback) all needed genuine
nested `if1_if` control flow — not a flat boolean AND-fold — so FA's
isinstance-narrowing could apply *before* type-checking a method call
against the narrowed type.

## Gaps

None. Every PEP 634 pattern kind, and every combination with
`case None:`, compiles and runs byte-identical to CPython on both
backends.

### `case None:` combined with another pattern — RESOLVED

`case None:` combined with a capture, a literal, `True`/`False`, or a
sequence/mapping/class pattern in the same match once required a
compile-time refusal (a shared clone coerced `None` to a falsy scalar
and the `None` arm matched the wrong subject). Two independent
mechanisms, both fixed 2026-07-21:

- **Mechanism 1** — `build_isinstance_call` (`python_ifa_build_if1.cc`)
  routed through the shared Python-level `isinstance()` wrapper, which
  FA generalized into one polymorphic clone across pattern kinds (the
  same bug class [closed/011](../ifa/issues/closed/011-setter-codegen-vs-analyzer-mismatch.md)
  fixed for `except` clauses). Switched to the raw `sym_primitive`
  isinstance send.
- **Mechanism 2** (the general one, not `match`-specific) — `None` and
  a raw scalar (`int`/`bool`/`float`) share the zero/NULL bit pattern
  in an unsplit contour, so a shared clone can't tell `None` from
  `0`/`False`. Root cause: `nil_type` was stripped from the AType
  `->type` projection unconditionally, so FA's type-splitter never
  separated `None` from a scalar. Fixed in `type_cannonicalize`
  (`ifa/analysis/fa.cc`) — nil is kept in `->type` when the union also
  carries a `num_kind` scalar, so FA gives `None` its own contour and
  the check folds statically. Full trace:
  [ifa/issues/060](../ifa/issues/060-none-branch-dropped-mixed-with-literal-bool-sequence.md).

With both fixes in, the compile-time guard
(`pattern_contains_none`/`pattern_is_risky_with_none` in
`build_match_pyda`) and its two helper functions were removed;
`tests/match_none.py` now exercises all four previously-blocked
combinations and matches CPython on both backends. The
[059](../ifa/issues/059-narrowing-peel-wrapper-boolean-collapse-gap.md)
narrowing fix (`peel_wrapper_def`) composes with this and remains in
place.

## What this unblocks

Real Python code using any PEP 634 pattern kind — including all three
rest-capture forms and every `case None:` combination — compiles and
runs correctly on both backends. No limitations remain.
