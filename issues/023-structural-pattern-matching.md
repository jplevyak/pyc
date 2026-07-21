# Issue 023: Structural pattern matching (`match`/`case`, PEP 634)

**Status:** open — every PEP 634 pattern KIND is implemented and
verified byte-identical to real `python3` output on both backends
(all landed 2026-07-12; positional class patterns added 2026-07-21;
re-verified same day, all tests below pass on both backends). What
remains are two narrow, explicitly-deferred features and one
runtime-crash limitation, all under "Gaps" below.
**Affects:** `python.g` (`match_stmt`/`case_block`/`case_guard`
grammar, soft-keyword `match`/`case`), `python_ifa_build_syms.cc`
(`mark_pattern_captures`, `collect_match_args`, `gen_class_pyda`),
`python_ifa_build_if1.cc` (`build_match_pyda`, `build_pattern_match`),
`ifa/if1/sym.h` (`Sym::match_args`).

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

Test coverage (all passing, both backends): `tests/match_basic.py`,
`match_capture.py`, `match_or.py`, `match_guard.py`, `match_seq.py`,
`match_map.py`, `match_class.py`, `match_class_positional.py`,
`match_literal_types.py`, `match_none.py`.

One lesson from this work worth remembering beyond match/case: FA
type-checks a call against a subject's *whole static union type*
across the entire program, not just within the runtime branch where
the type actually narrowed. Sequence/mapping/class pattern matching
(and the pre-existing literal-pattern fallback) all needed genuine
nested `if1_if` control flow — not a flat boolean AND-fold — so FA's
isinstance-narrowing could apply *before* type-checking a method call
against the narrowed type.

## Gaps

Two explicitly-deferred features. Both fail loudly (parse error) —
neither is silent:

- **Star capture in sequence patterns** (`case [a, *rest]:`) — same
  underlying grammar gap as issue 024's extended-unpacking assignment
  targets.
- **`**rest` in mapping patterns** (`case {"k": v, **rest}:`) —
  `python.g`'s `dictorsetmaker` has no `'**' NAME` alternative (real
  Python's `{**other}` dict-merge literal isn't a pyc feature
  either).

One runtime-crash limitation, actively guarded against at compile
time:

### `case None:` combined with almost any other pattern crashes at runtime

`case None:` may only be combined with a wildcard (`case _:`) and/or
other `case None:` arms in the same match. Combined with anything
else — a capture, a literal, `True`/`False`, or a sequence/mapping/
class pattern — compiled code crashes with `Assertion '!"runtime
error: matching function not found"'`. `build_match_pyda` detects
this at compile time (`pattern_contains_none`/
`pattern_is_risky_with_none`, scanning every case pattern in the
match) and refuses to compile it, pointing at the workaround (split
into a separate match statement, or use `case x if x is None:`).
Re-confirmed reproducing 2026-07-21.

**Root cause, as far as traced:** dispatching `__str__` (needed by
`print()`) on a subject whose static type spans multiple
PRIMITIVE/boxed types (`None | int | float | ...`) — none of which
carry the classtag mechanism polymorphic dispatch relies on for class
instances. Ruled out as the same bug as (closed)
[026](closed/026-polymorphic-method-dispatch-partial-override-crash.md)
— identical assertion text, but 026 was a `Type_SUM`-typed
class-instance receiver hitting a classtag gap; this is a union of
primitive/boxed types with no classtag involved at all (confirmed via
`PYC_DBG_DISPATCH=1`). Also confirmed the crash is not specific to:
which lowering `case None:`'s own check uses (three different forms
tried, all fail identically); whether the *other* case does any type
narrowing (a plain unconditional capture fallback crashes too); or
`match`/`case` itself (a plain `if val is None: ... elif
isinstance(val, int): ...` compiles and runs fine). It's specific to
the chain of nested `if1_if`s `build_match_pyda` generates for a
multi-arm match over a subject union that includes `None`. Points at
a genuine codegen gap in dispatching a method over a primitive/boxed
union, not anything fixable from this file's own lowering code. Not
yet filed as its own `ifa/issues/` entry — the compile-time refusal
keeps it from being a correctness hazard meanwhile.

## What this unblocks

Real Python code using any PEP 634 pattern kind compiles and runs
correctly on both backends. What's left — three deferred features and
one guarded runtime limitation — are all narrow and loud, not
correctness traps.
