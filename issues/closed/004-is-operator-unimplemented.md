# Issue 004: `is` operator is unimplemented; blocks recursive-type `None`-narrowing

**Status:** **fully fixed June 2026.**

- `x is None` / `None is x` lower to
  `prim_isinstance(x, sym_nil_type)` (frontend); codegen
  emits a NULL pointer check.
- `x is y` (non-None operands) lowers to `prim_is(x, y)`
  (new primitive, id 56); codegen emits pointer equality
  `(void*)x == (void*)y` in C and `CG2_BINOP EQ` in v2
  LLVM.  Added during issue 028 step 4 — the earlier
  `__pyc_any_type__::__is__` always-False fallback
  silently broke ring-detection idioms like
  `z.next is z`.
- `None is None` / `None is not None` fold to True/False
  constants at the frontend.

`is`/`is not` no longer dispatch through any method.  The
`__is__` / `__nis__` stubs on `__pyc_any_type__` and
`__pyc_None_type__` remain only for explicit
`x.__is__(y)` calls.

See [ifa/issues/024](../../ifa/issues/closed/024-is-comparison-narrowing.md)
for the narrowing-side fix and
[ifa/issues/028](../../ifa/issues/closed/028-fibheap-blockers.md)
for step 4's identity primitive.

**Affects:** pyc Python frontend (`python_ifa_build_if1.cc:102`
maps `PY_CMP_IS` to a `__is__` symbol dispatch) + the
`__pyc__` builtin module (no class defined `__is__` until
this commit).
**Surfaced while:** writing recursive-type tests for
[issue 023](../../ifa/issues/closed/023-v2-is-value-type-consumer.md)
escape coverage — the natural pattern
`if node is None: return 0` fails to compile.

## Symptom

Any `is` comparison fails to dispatch.  Minimal repro:

```python
a = 5
b = 5
print(a is b)
```

Errors:
```
_is1.py:3: illegal call argument type 'a' illegal: int64
_is1.py:3: illegal call argument type 'b' illegal: int64
_is1.py:3: unresolved call '__is__'
```

Class instances fail the same way:

```python
class C: pass
a = C()
b = a
print(a is b)        # → unresolved call '__is__'
```

The downstream impact: any recursive-type pattern that uses
`x is None` to narrow a `Node | None` union (linked lists,
trees, generic recursive data structures) can't compile.

## Root cause

`python_ifa_build_if1.cc:102`:

```cpp
case PY_CMP_IS: return make_symbol("__is__");
```

The frontend translates `x is y` to a `__is__(x, y)` symbol
dispatch.  But no class in `__pyc__/*.py` defines `__is__`:

```
$ grep -rn "__is__" __pyc__/
$ # (no matches)
```

So IFA's dispatch resolution finds no matching function,
emits `unresolved call '__is__'`, and the expression has no
type.

Note: pyc handles `is not` the same way (`PY_CMP_IS_NOT`),
which would inherit the same gap if filed too.

## Proposed fix

Add `__is__` (and `__is_not__`) to `__pyc_any_type__` in
`__pyc__/00_runtime.py`:

```python
class __pyc_any_type__:
  ...
  def __is__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("=="), x)
  def __is_not__(self, x):
    return __pyc_operator__(__pyc_symbol__("!"),
                            __pyc_operator__(self, __pyc_symbol__("=="), x))
```

`__pyc_operator__` with `==` produces a C-level `==` comparison.
For class instances (ptr-shaped), this is pointer equality —
exactly Python's `is` semantics for non-numeric types.  For
numeric types, `is` compared with `==` is close enough for
the constant-fold cases pyc handles; CPython's int-interning
quirk for small ints isn't a meaningful difference here.

The `__pyc_None_type__` class will inherit from
`__pyc_any_type__` (it implicitly already does — every class
specializes from it).  So `node is None` works regardless of
whether `node` is a Node or the None singleton: both have
`__is__` and the dispatch resolves to identity comparison
on each branch.

## What landed (partial fix)

Added `__is__` and `__nis__` to both `__pyc_any_type__`
(default: returns False / True respectively) and
`__pyc_None_type__` (returns `x.__null__()`).  Also added a
`__null__` method to `__pyc_any_type__` so the default-False
case has somewhere to live.

```python
class __pyc_any_type__:
  def __null__(self):   return False
  def __is__(self, x):  return False
  def __nis__(self, x): return True

class __pyc_None_type__:
  def __null__(self):   return True
  def __is__(self, x):  return x.__null__()
  def __nis__(self, x):
    if x.__null__(): return False
    return True
```

This unblocks `is None` patterns where pyc IFA can dispatch
cleanly:
- `a = None; b = None; print(a is b)` → True ✓
- `c = 5; print(c is None)` → False ✓
- `c = 5; print(c is not None)` → True ✓
- `a = None; c = 5; print(a is c)` → False ✓ (when isolated)

## What's still broken

Two patterns trigger an IFA "matching function not found"
assertion at codegen:

1. **Polymorphic `None`-LHS dispatch.**  When `None`
   appears as the LHS of `is` against two distinct RHS
   types in the same module:
   ```python
   a = None
   b = None
   c = 5
   print(a is b)   # OK
   print(a is c)   # crash
   ```
   The IFA dispatch on `__pyc_None_type__.__is__` tries to
   specialize over a polymorphic `x` (union of None and int)
   and can't decompose the union for `x.__null__()`.

2. **Recursive-type narrowing.**  The originally-motivating
   pattern:
   ```python
   class Node:
     def __init__(self, v):
       self.value = v
       self.next = None
   def sum_list(node):
     if node is None: return 0
     return node.value + sum_list(node.next)
   ```
   compiles, but at runtime hits the "matching function not
   found" assertion because IFA doesn't narrow `node` to
   the non-None branch after the `is None` check.

Both root-cause to the same gap: **pyc's IFA splitter
doesn't narrow union types based on `is`-comparison
outcomes**, and it doesn't decompose union types for the
inner method dispatch.

These need IFA-level work, not __pyc__ changes.  Filed as
[ifa/issues/024](../../ifa/issues/closed/024-is-comparison-narrowing.md).

## Verification plan

1. Minimal repro `a = 5; b = 5; print(a is b)` compiles and
   prints `True`.
2. Class identity: `c = C(); print(c is c)` → `True`,
   `print(c is C())` → `False`.
3. None-narrowing: a recursive linked-list test similar to
   `tests/recursive_alloc_basic.py` that uses
   `Node-with-Node-field` and `if node is None: return 0`
   compiles and runs correctly.  The narrowing should let
   `node.value` typecheck in the non-None branch.
4. Pyc suite stays at 85/0 across both backends.
5. CPython cross-verify: identity semantics match where
   feasible (modulo small-int interning, which pyc doesn't
   need to replicate).

## What this unblocks

- Recursive data structures with `None`-terminated chains
  (the natural Python pattern for linked lists, trees,
  graphs).
- A full fibonacci-heap test (the original motivation for
  the recursive-allocation test set landed in commit
  `9dee759`).
- Any test that mirrors idiomatic Python's "optional
  reference" pattern.
- A class of pyc programs ported from CPython that uses
  `is None` for sentinel checks.

## Related

- `python_ifa_build_if1.cc:102` — the `PY_CMP_IS` →
  `__is__` mapping that needs a matching method.
- `__pyc__/00_runtime.py` — where the method should land
  (in `__pyc_any_type__` so every type inherits it).
- `__pyc__/02_numeric.py:63` — example of `__pyc_operator__("==",...)`
  patten that `__is__` would mirror.
- `tests/recursive_alloc_basic.py` and siblings — the tests
  that side-stepped the issue by not using `is None`.
