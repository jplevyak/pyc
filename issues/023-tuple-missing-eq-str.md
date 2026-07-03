# Issue 023: `tuple` has no `__eq__`, `__str__`, or `__repr__`

**Status:** open.
**Affects:** `__pyc__/04_sequence.py` (`class tuple`).
**Related:** discovered while verifying
[022-builtin-type-zero-arg-constructor-broken.md](closed/022-builtin-type-zero-arg-constructor-broken.md)'s
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

(expected `(1, 2, 3)`; CPython's actual output). `list`/`dict`/`set`
all have working `__eq__` and `__str__`/`__repr__` — `tuple` never
got either.

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

## Verification plan

1. `(1, 2) == (1, 2)` → `True`, `(1, 2) == (1, 3)` → `False`.
2. `print((1, 2, 3))` → `(1, 2, 3)`; `print(())` → `()`;
   `print((1,))` → `(1,)`.
3. A program with two or more separate empty-tuple construction
   sites (direct `print(tuple())` plus `t = tuple(); print(t)`, or
   the literal-syntax equivalent) compiles cleanly on both backends
   with no crash and no spurious warnings — the specific pattern
   that crashed the C backend during this investigation.
4. Existing tuple tests continue to pass unchanged.

## What this unblocks

`tuple` is a basic Python type; comparing tuples (common for
multi-value returns, dict keys, sorting) and printing them for
debugging are everyday operations that currently fail or produce
useless output.
