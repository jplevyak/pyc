# 035 — `P_prim_set_index_object` casts the assigned value with no compatibility check; a genuine `set`-element type union remains open

**Status:** partially fixed 2026-08-06. The fatal compile-error part
(both branches of `P_prim_set_index_object`) is fixed and verified.
`shedskin_examples/tictactoe/tictactoe.py` now compiles clean with
zero warnings on both backends — it does **not** yet run to
completion; see "What's still open" below.
**Affects:** `ifa/codegen/cg.cc`'s `P_prim_set_index_object` (both
the general list branch and the fixed-size tuple-list constant-index
branch).
**Related:** [056](056-degraded-index-type-raw-c-compile-error.md) —
the established precedent and convention this issue extends to a new
call site (the *value* being stored, not the index argument, which
056 already covers at the same two call sites);
[077](../ifa/issues/closed/077-primitive-equality-codegen-missing-salvage-guard.md)/[034](closed/034-iadd-fallback-and-mixed-numeric-regression.md)
— the same "num_kind-based scalar/pointer tolerance" pattern, applied
here for the third time at a fourth call site.

## Symptom

`tictactoe.py` failed to compile:

```
tictactoe.py.c:3972:88: error: cannot cast from type '_CG_float64' (aka 'double') to pointer type '_CG_void' (aka 'void *')
 3972 |   ((_CG_void*)(_CG_list_ptr(t155)))[_CG_norm_idx(t92,(int32)_CG_prim_len(0,t155))-0] = (_CG_void)t156;
```

Ten occurrences, all the same shape: a list whose element type
resolved to `_CG_void` (this list's storage layout, e.g. a
salvage-degraded or genuinely heterogeneous union collapsed to a
generic/boxed representation) receiving a value that resolved to a
concrete scalar (`_CG_float64`). Casting a `double` directly to a
pointer type is invalid C (unlike two pointer types, or two scalars
of different width/kind, which are always castable) — a genuine
`pyc`-produced compile error, the same bug *class* 056 already named
and fixed for the *index* argument at these same two call sites, now
found at the *value* argument instead.

## Root cause

`P_prim_set_index_object` (`cg.cc`) emits `((ety*)(...))[idx] =
(ety)value;` — casting the assigned value directly to the list's
element C type — with no check that the value's actual resolved type
is compatible with that cast. Reachable via two branches: the general
(dynamic list/negative-index-normalized) branch, and a separate
fixed-size tuple-list constant-index branch (`((T)list)->e<N> =
value;`) for lists whose literal size makes them internally
struct-like — both had the identical unguarded-cast shape.

## Fix

Both branches now check, before emitting the cast: is exactly one of
{element/field type, value's resolved type} a scalar (`num_kind`
truthy) and the other not? If so — a genuine pointer/scalar
mismatch — degrade to `assert(!"runtime error: list element type
mismatch")` (or `fail(...)` when `fruntime_errors` is off) instead of
emitting the cast. Two scalars of any kind/width, or two
pointer-representable types, are always compatible (mirrors
077/034's identical tolerance at sibling call sites) — only the
scalar-vs-pointer crossing is flagged.

## What's still open

With the compile fix alone, `tictactoe.py` compiles clean but the
binary aborts at runtime on this same new guard — a **genuine**
`set`-element type divergence, not a salvage artifact: `set`'s
`_items` field ends up needing to hold both `int64` (from
`set(fields).difference(set([0]))`, `doRow`'s all-int board-cell
sets) and `float64` (traced to `set.__init__`'s `_items` literal
resolving to `_CG_prim_list(_CG_float64, 0)` in a *different*,
internally-shared `set()` construction reachable through `set`'s own
generic `union()`/`intersection()`/`__pyc_set_from_iterable__`
methods — these get analyzed for their own type as part of building
every class's method table regardless of whether any call site in
this specific program ever dispatches to them, similar in shape to
[ifa/071](../ifa/issues/closed/071-chess-accumulated-union-notype-cascade.md)'s
"accumulated union, no single root cause" pattern rather than a
narrow, one-off bug). Not fully traced — the exact call site
supplying the float remained unidentified after a `grep`-based
search; would need the same kind of FA-level instrumentation 071's
dig used to pin down precisely.

**A promising-looking lead was tried and reverted.**
`__set_iter__`/`__dict_iter__`/`__dict_items_iter__`
(`__pyc__/08_set.py`/`07_dict.py`) still had the exact class-body-
default-plus-`__init__`-override shape
[076](../ifa/issues/closed/076-mutation-driven-receiver-divergence-not-cloned.md)
fixed for `dict`/`set` themselves — flagged at the time as "not
surveyed" in
[078](../ifa/issues/closed/078-class-body-default-plus-init-override-permanently-unions.md).
Removing those class-body defaults (mirroring 076's exact fix)
**did** additionally fix `shedskin_examples/loop/loop.py` (FAIL →
`COMPILED_C`) — but did **not** fix `tictactoe.py`'s runtime crash
(identical assert, identical contour), and **caused a real
regression**: `shedskin_examples/webserver/webserver.py` (fixed by
[032](closed/032-dict-view-membership-missing-contains.md), earlier
this session) went from compiling clean back to a hard `_CG_str_eq`
compile error (`incomplete type '_CG_any'` where `const char *` was
expected). Reverted per this session's established rule (verify
first, ship only if zero regressions) rather than trade one corpus
win for another corpus loss. `__pyc__/08_set.py`/`07_dict.py` are
unchanged from before this issue. Whoever picks this up next should
budget for both: (a) finding *why* removing those class-body
defaults broke `webserver.py` specifically (a `dict`-side effect,
plausibly involving `__dict_iter__`'s own field precision feeding
into something `parseParams`/`headers.keys()` relies on) before
re-attempting it, and (b) the deeper `set`-element-union dig above,
independently of whether (a) is resolved.

## Verification

- `ifa --test`: 58/58.
- `tests/list_element_type_mismatch_salvage.py` (new): ordinary,
  uniformly-typed list/tuple-list mutation through both fixed
  branches, confirming the new guard doesn't disturb normal usage.
  Compiles with zero warnings, output matches `python3` exactly.
- `tictactoe.py`: compiles with **zero** warnings on both backends
  (was a hard compile error); does not yet run to completion (see
  above).
- `test_pyc.py`, C and LLVM backends, `PYC_CSM` unset: 243/11/0/4
  both (242 baseline + 1 new test, 0 regressions).
- `test_pyc.py`, C and LLVM backends, `PYC_CSM=2`: 239/11/4/4 both,
  same 4 pre-existing failures.
- `shedskin_sweep.sh`, both `PYC_CSM` settings: one clean, isolated
  gain each (`tictactoe`: `FAIL` → `COMPILED_C`), zero regressions,
  diffed directly against saved pre-fix `results.tsv` — confirmed
  *after* reverting the `__set_iter__`/`__dict_iter__` attempt, so
  this reflects only the `P_prim_set_index_object` guard.

## What this unblocks

Any program where a list's element type ends up genuinely or
speculatively pointer/scalar-mismatched against a stored value no
longer hits a hard build failure — matches the established
compile-clean-but-may-runtime-assert convention (issue 056) already
applied at every other salvage-reachable call site this investigation
has covered. Doesn't fix the underlying `set`-element-union precision
gap (or the `__set_iter__`/`webserver.py` interaction) — both remain
open, described above for whoever picks them up next.
