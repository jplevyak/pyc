# Issue 023: `tuple` has no `__eq__`, `__str__`, or `__repr__`

**Status:** fixed. See "What landed" below.
**Affects:** `__pyc__/04_sequence.py` (`class tuple`).
**Related:** discovered while verifying
[022-builtin-type-zero-arg-constructor-broken.md](022-builtin-type-zero-arg-constructor-broken.md)'s
fix — `tuple()` itself now works, but printing or comparing the
result (or any other tuple) fails; confirmed unrelated to the
zero-arg case specifically, since non-empty tuple literals hit the
same gaps.

## Symptom

```python
print((1, 2) == (1, 2))
```

```
warning: unresolved call '__eq__'
_023.py:1: illegal call argument type expression illegal: tuple
_023.py:1: illegal call argument type expression illegal: tuple
_023.py:1: illegal call argument type expression illegal:
_023.py:1: expression has no type
```

```python
print((1, 2, 3))
```

```
<instance>
```

(expected `(1, 2, 3)`; CPython's actual output). `list`/`set` have
`__str__`/`__repr__`; `tuple` never got either. `list.__eq__` turned
out to have its own latent bug (see "What landed"), and `dict` turns
out to have no `__eq__` at all either (filed separately as
[024-dict-missing-eq.md](024-dict-missing-eq.md)) — this issue's
"comparison operators are inconsistently supported across containers"
framing was more widespread than originally scoped.

A `__str__` was attempted and reverted during issue 022's work
(mirroring `list.__str__`'s `for k in range(0, len(self)): x +=
self[k].__repr__()` shape) — it printed non-empty tuples correctly,
but **crashed the C backend compiler outright** (segfault, not just
a diagnostic) on a specific pattern: two or more call sites
constructing an always-empty tuple in the same module (e.g. one
direct `print(tuple())` plus a separate `t = tuple(); print(t)`).
The LLVM backend didn't crash but emitted the same "expression has
no type" / "unresolved call '__iadd__'" warnings for that pattern.
This suggests whatever `tuple.__str__` is added needs to handle the
zero-length case more carefully than list's did. Confirmed the
crash is tuple-specific, not a general "empty generic container"
bug: the analogous pattern (`print(list()); l = list(); print(l)`)
compiles and runs cleanly on both backends with existing
`list.__str__` — no warnings, no crash.

## Proposed fix sketch

1. Add `tuple.__eq__`/`__ne__`, likely a per-element comparison loop
   (see `list.__eq__`, `__pyc__/04_sequence.py:67`, as the model —
   check whether tuple's fixed arity vs list's variable length
   changes anything).
2. Add `tuple.__str__`, mirroring `list.__str__` but with Python's
   trailing-comma-for-1-tuple convention (`(1,)` vs `(1, 2)` vs
   `()`) — but first reproduce and fix the C-backend segfault noted
   above on the multi-empty-tuple-call-site pattern before landing
   this, or the fix will trade a cosmetic bug for a crash bug.

## What landed

**Root cause of the C-backend segfault (found via `gdb bt`, not
guessed):** `ifa/codegen/cg.cc:223`, `write_c_prim`'s `P_prim_make`
handling for an empty tuple construction (`n->rvals.n < 4`, i.e.
zero elements) dereferenced `n->lvals[0]->type->element->type`
without checking `element` for null. An always-empty tuple can have
its element type genuinely unresolved by FA (tuples are
heterogeneous, so there's no single "element type" the way `list`
has one) — `element` itself was null, not just `element->type`, and
the crash was a straightforward null-pointer dereference, not
infinite recursion (initial suspicion, given the deep-looking
`write_c_pnode` recursion in the backtrace — a red herring; that
recursion was finite, ~30 frames, and terminated normally in
isolation with the `is`-comparison fixed below). Fixed by guarding
`elem` for null before accessing `elem->type`, treating "no element
type" the same as "void element type" (matches the existing
`voidish` fallback's intent).

**A second, independent, real bug found and fixed along the way**
(not the segfault's cause, but hit while reproducing it): `PNode*`
was missing a `PointerHash` specialization — the same bug class as
issue 021's `Var*` finding. `write_c_pnode`'s `done` set
(`Vec<PNode*>`, `cg.cc:838`) and `PNode`'s own `cfg_succ`/`cfg_pred`/
`phi`/`phy` fields all use `Vec<PNode*>` as a set/array; `PNode` has
a monotonic `id` (`pnode.cc`) like the other six id-bearing types
`ifa/notes/004`'s "options A+B" specialized, but was left out.
Fixed with the same one-line pattern in `ifa/if1/pnode.h`. This
alone did not fix the segfault (confirmed by testing before and
after the `cg.cc` null-check fix independently) but is a real,
separate, low-risk correctness improvement in its own right, given
`PNode*` is used as a `Vec`-as-set key throughout the CFG/SSU
machinery, not just in `write_c_pnode`.

**`tuple.__eq__`**, added (per-element comparison after a length
check, using `len()` — not `.len()`, see next paragraph).

**`list.__eq__` had its own latent bug**, found while using it as
this fix's model: it called `l.len()`/`self.len()` — not a real
method (`list` has no `len` attribute) — instead of the `len()`
builtin. This silently returned the *wrong* answer for
different-length lists (confirmed: `[1, 2] == [1, 2, 3]` returned
`True`) rather than failing to compile, so it went unnoticed by any
existing test (none happened to compare different-length lists).
Fixed alongside tuple's, using the same `len(x)` builtin form.

**`__ne__` added to both `list` and `tuple`**, mirroring
`set.__ne__`'s existing `return not self.__eq__(other)` shape — both
were missing it entirely (only `set` had it before this fix).
`dict`'s equivalent gap (no `__eq__` *or* `__ne__` at all) turned out
to need a different, key-order-independent implementation shape, so
it's tracked separately as
[024-dict-missing-eq.md](024-dict-missing-eq.md) rather than folded
in here.

**`tuple.__str__`** added, mirroring `list.__str__`'s per-element
loop, with Python's trailing-comma-for-1-tuple convention
(`(1,)` vs `(1, 2)` vs `()`).

Verified against CPython on both backends: non-empty and empty
tuple literals and `tuple()` calls, `__eq__`/`__ne__` for
same-length/different-length/equal/unequal cases, the exact
multi-empty-tuple-construction-site pattern that originally
crashed the C backend (`tests/tuple_empty_regression.py`). Added
`tests/tuple_eq_str.py`, `tests/tuple_empty_regression.py`, and
`tests/list_tuple_eq_ne.py`. Full `./test_pyc` +
`PYC_FLAGS="-b" ./test_pyc`: 119/0 both backends, no regressions.
`ifa`'s own `./ifa --test` (58/0) and `make test` (same 6
pre-existing, unrelated `patterns`-phase failures confirmed earlier
this session) both unaffected by the `cg.cc`/`pnode.h` changes.

## What this unblocks

`tuple` is a basic Python type; comparing tuples (common for
multi-value returns, dict keys, sorting) and printing them for
debugging are everyday operations that no longer fail or produce
useless output. The `cg.cc` null-check fix also removes a real
compiler crash (not just a diagnostic gap) that would have hit any
future code constructing an always-empty tuple, independent of this
issue's `__eq__`/`__str__` additions.
