# 038 — `pyc_program_has_raise` never armed for an ordinary call into a builtin method that raises (uninitialized-memory read, zero warnings)

**Status: FIXED 2026-08-06.** Found while digging into
[ifa/049](../../ifa/issues/049-raise-only-contour-notype.md) at the
user's request; turned out to be the actual, sole cause of
`shedskin_examples/sudoku2/sudoku2.py`'s remaining runtime blocker
(previously mis-attributed to 049's own mechanism in
[037](037-sudoku2-str-ne-void-cast-and-str-index.md)'s "what's still
open" section — see the correction there).
**Affects:** `python_ifa_build_syms.cc` (new `collect_raise_names`/
`collect_builtin_raise_names`/`ast_reaches_raise`/
`user_code_reaches_raise`), `python_ifa_main.cc` (two call sites),
`python_ifa_int.h` (declarations).
**Related:** [037](037-sudoku2-str-ne-void-cast-and-str-index.md) —
`str.index()`, added there, is what exposed this (a builtin method
that raises, invoked via an ordinary call — a shape that didn't exist
in `__pyc__` before that fix); [ifa/049](../../ifa/issues/049-raise-only-contour-notype.md)
— the separate, still-open FA bug this was originally (wrongly)
thought to be part of.

## Symptom

Any program whose *only* reachable raise is through an ordinary call
into a builtin method — not a bare `raise`, `assert`, or `yield`
anywhere in user code — compiled with **zero warnings** and then
silently read **uninitialized memory** at runtime instead of ever
raising:

```python
s = "2....64.1"
n = 0
for digit in "123456789":
    try:
        n += s.index(digit)   # str.index, issues/037
    except ValueError:
        n -= 1
print(n)
```

prints `14` (matching CPython) before this fix; a large garbage
integer after `str.index()` was added but before this fix. No
compiler diagnostic of any kind pointed at the problem.

## Root cause

`pyc_program_has_raise` (`python_ifa_build_if1.cc`) is the whole-
program gate `emit_exc_check` reads before emitting *any*
exception-checking code at all — including the post-call
`isinstance(__pyc_exc__, NoneType)` check every ordinary call needs so
a caller can notice a callee raised. It's armed by `build_syms_pyda`
at exactly five user-code AST shapes: bare `raise`, `assert`,
`yield`, `yield` expr, `yield from`. Each deliberately re-arms the
gate at the point *user* code becomes reachable to a *builtin*
raiser — `__pyc_assert_fail__`'s own `raise AssertionError` for
`assert`, a generator's `StopIteration` for the three `yield` forms —
because builtin-module raises are excluded from arming the gate
directly (`__pyc_assert_fail__`'s raise is loaded into every program
regardless of whether user code calls `assert`; arming unconditionally
there would defeat the whole point of the gate).

An *ordinary call* from user code into a builtin method that raises
never got the same treatment — no sixth AST shape existed for "this
call's target raises." `str.index()` ([037](037-sudoku2-str-ne-void-cast-and-str-index.md))
is exactly this shape: a builtin method, reached via a plain method
call (`s.index(digit)`), not one of the five special forms. A program
whose only raise is reachable that way left `pyc_program_has_raise`
false, so `emit_exc_check` emitted nothing anywhere in the program —
including around the user's own `try`/`except` — and `str.index`'s own
raise path (correctly, per `goto_exc_target`'s design: "leave
`fn->ret` undefined here, the caller's check reads it first" — see
[ifa/049](../../ifa/issues/049-raise-only-contour-notype.md)'s root
cause section) became a read of a C local that was never assigned,
because the check that would have short-circuited before that read
was never emitted in the first place.

## Fix attempts

**First attempt (reverted):** a scanner mirroring
`collect_can_raise`'s own conservative "unresolved call → assume it
raises" rule, using `Sym::can_raise` (transitive) for resolved plain
calls. Regressed hard: `Sym::can_raise` conflates "this function
directly raises" with "this function calls something unresolved,
e.g. a polymorphic `__repr__` dispatch" — correct for its actual
consumer (`known_callee`, a per-call-site check-elision optimization,
where over-approximating is cheap) but far too broad for a
whole-program gate. `print` doesn't raise directly but internally
dispatches to unresolved `__repr__`/`__str__` implementations, so its
`can_raise` is true — arming the gate for every program that calls
`print` (nearly all of them) regressed five `--test_scoping` golden
traces (a new `__pyc_exc__` symbol lookup appears in the scope trace)
and, more seriously, **broke async/coroutine codegen outright**
(`co_await t4` on a `_CG_nil_type` — inserting exception-check control
flow into an async body in a new place isn't safe in general). The
"unresolved call → conservative true" half made it worse on its own:
virtually any non-trivial program calls at least one method.

**Landed fix:** `Sym::direct_raise` instead of `Sym::can_raise` — set
only when a function's *own* body textually contains a `raise`
(`build_if1_pyda`'s `PY_raise_stmt` case), never propagated through
calls. Method calls (`s.index(x)`) aren't resolvable to a specific Sym
pre-FA — any class could define `.index` — so they're matched by
**name** instead: `collect_builtin_raise_names` walks the builtin
module once, after its own `build_if1` has run (so `direct_raise` is
populated), collecting every method/function name with `direct_raise`
set (interned via `cannonicalize_string`). `user_code_reaches_raise`
then scans user code for either a plain call resolved to a Sym with
`direct_raise`, or a method call whose attribute name is in that set,
called once from `ast_to_if1_extend` right after `compute_can_raise
(user_mods, ctx)`. This precisely catches `str.index()` ("index" is in
the set) without touching `print` (not in the set — `print` is a
native compiler intercept, `sym_print`, not a `PY_funcdef`-backed Sym
at all, never a `direct_raise` candidate) or anything that merely
*transitively* depends on something raise-capable. User-level direct
raisers need neither path: a user function/method with its own
`raise` already arms the gate unconditionally via the existing
`PY_raise_stmt` arm, independent of whether it's ever called.

## Verification

- Repro above: zero warnings, output `14` matching `python3` exactly
  (was silent garbage).
- `test_pyc.py`, C and LLVM backends, `PYC_CSM` unset: 248/11/0/4 both
  — the first (reverted) attempt had regressed 11 of these
  (`scope_*`, `async_*`); the landed fix restores all of them exactly.
- `test_pyc.py`, C and LLVM backends, `PYC_CSM=2`: same 4 pre-existing
  failures both, no new ones.
- `ifa --test`: 58/58.
- `shedskin_sweep.sh`: clean before/after from the same commit
  (stash/sweep/pop/rebuild/sweep). **Single line changed across the
  entire 77-example corpus**, and it's cosmetic: `rsync.py`'s
  pre-existing `'matchblock' has no type` warning shifted one source
  line, same `COMPILED_C_WARN` status before and after.
- `sudoku2.py`: **now runs to completion**, output byte-identical to
  `python3` (was documented in
  [037](037-sudoku2-str-ne-void-cast-and-str-index.md) as compiling
  clean but segfaulting — that segfault was misdiagnosed there as
  [ifa/049](../../ifa/issues/049-raise-only-contour-notype.md)'s
  mechanism; it was entirely this bug, now closed. See the correction
  in 037.

## What this unblocks

- `shedskin_examples/sudoku2/sudoku2.py` (a corpus benchmark): the
  segfault documented as an open blocker in issue 037 is resolved —
  sudoku2 is now fully fixed, not partially.
- Any *future* builtin method added to `__pyc__` that raises, reached
  only via ordinary user-code calls (not `assert`/`yield`) — the
  general shape `str.index()` happened to be first to hit. Fixed once,
  generally, not just for `str.index()`.
- Corrects `ifa/049`'s documentation, which had folded this bug's
  symptom in as a "third, even simpler" trigger shape for that FA bug.
  It wasn't — see the correction added there.
