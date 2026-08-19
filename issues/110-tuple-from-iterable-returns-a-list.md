# 110 — `tuple(iterable)` returns a list

**Status:** open, found 2026-08-19 while tracing `sunfish`'s runtime
abort. Repro: `tests/tuple_from_iterable_is_list.py` (`.known_issue`).

## Symptom

```python
row = [1, 2, 3]
padded = (0,) + tuple(x + 1 for x in row) + (0,)
print(padded)
```

| | result |
|---|---|
| CPython | `(0, 2, 3, 4, 0)` |
| **pyc** | **`[0, 2, 3, 4, 0]`** — a list |

## Cause — a documented compromise

`python_ifa_build_if1.cc`:

```cpp
// Established compromise: zip/map/filter/enumerate/reversed
// already return lists; indexing/iteration/len are identical,
// printing/hashing differ.
if (f == sym_tuple && pos_args.n == 1) {
  call_method(&ast->code, ast, a0->rval, make_symbol("__pyc_tolist__"), ast->rval, 0);
}
```

A one-argument `tuple(x)` is lowered to `x.__pyc_tolist__()`.

## Why it is worth revisiting now

1. **The output is visibly wrong.** `printing/hashing differ` understates
   it: any program that prints a `tuple(...)` result prints a list.
2. **It manufactures a `{list, tuple}` union.** `sunfish`'s
   `padrow = lambda row: (0,) + tuple(x+piece[k] for x in row) + (0,)`
   yields a *list*, while its `pst` values also come from tuple literals
   and `()`. `pst[k]` therefore holds both, and any shared method on that
   union — slice, `len`, index, iterate — has two candidates with the
   same C-level receiver type and no runtime tag, so it aborts
   (`tests/list_tuple_union_method.py`,
   [030](../ifa/issues/030-DISPATCH-polymorphic-dispatch-fat-pointers.md)).
   shedskin types the whole program `dict<str*, tuple<__ss_int>*>` with
   **no union at all**, precisely because its `tuple(iterable)` returns a
   tuple.
3. **The blocker that motivated the compromise is gone.** Returning a
   real tuple used to be impossible for an iterable of unknown length,
   because pyc's tuples were fixed-arity records. As of
   [ifa/issues/109](../ifa/issues/109-mixed-arity-tuple-slice-dispatch.md)
   a tuple CreationSet whose generic element is populated takes **list
   layout** — variable length, known element type. So a genuine
   variable-length tuple is now representable.

## Fix direction

Lower `tuple(x)` to something that produces a **tuple** whose CreationSet
takes list layout — the same representation `tuple.__pyc_getslice__` now
yields — rather than to `__pyc_tolist__`. The runtime layout is already
shared (`cg.cc` builds every tuple with `_CG_prim_tuple_list`, which sets
a real list header), so this is about the *type* the frontend assigns,
not about storage.

Watch the same trap 109 hit: `sizeof_element` needs the element sym,
which `PYC_TUPELEM` (now default) supplies.

## Verification plan

- `tests/tuple_from_iterable_is_list.py` prints `(0, 2, 3, 4, 0)` and
  `(0, 2)`; delete its `.known_issue` tag.
- `tests/list_tuple_union_method.py` is unaffected (it builds its union
  explicitly, so it stays a genuine 030 case).
- `sunfish` compiles **and runs**.
- Corpus: no exit-code changes, measured against a freshly taken
  baseline. The named beneficiaries `genetic2` (`tuple([TreeNode() ...])`)
  and `chess` (`tuple(range(...))`) must not regress — they are the two
  the compromise was originally made for.


## Attempted 2026-08-19 — the library-level construction does not work

The plan was to keep the frontend intercept's shape and only change what
it dispatches to: `tuple(x)` → `x.__pyc_totuple__()`, with

```python
def __pyc_totuple__(self):
    t = ()
    for x in self:
        __pyc_primitive__(__pyc_symbol__("merge_in"), t, x)   # flow element types in
    return __pyc_c_call__(__pyc_primitive__(__pyc_symbol__("merge"), t, t),
                          "_CG_list_getslice", list, self, ...)
```

The reasoning was that `merge_in` (the primitive `append` uses) would
populate the empty tuple's generic element, making `tuple_able()` false
and so giving the CreationSet list layout — variable length, known
element type — with `merge(t, t)` naming that tuple type as the return.

**It compiles and is wrong**: the reproducer prints `[0, 0]` against
CPython's `(0, 2, 3, 4, 0)` — still a list, and now with wrong values
too. `merge(t, t)` on an empty tuple literal names the *empty-tuple*
type; populating an element through `merge_in` did not turn it into the
variable-length tuple the return needs, and the copy went wrong as well.
Reverted; baseline restored and re-verified.

### What that tells us

The blocker is not the *representation* — ifa/issues/109 established that
a tuple CreationSet with a populated element does take list layout, and
`tuple.__pyc_getslice__` returns exactly such a value today. The blocker
is **constructing a value of that type from `__pyc__` Python source**:
there is no expression that names "tuple, variable length, element type
E". A tuple literal names a fixed arity; `merge`/`merge_in` on one does
not generalise it.

So this needs a frontend or FA-level construction — a primitive that
makes a tuple CreationSet whose element is seeded from an iterable's
element — rather than a `__pyc__` library edit. That is the same shape as
`P_prim_make`, which builds a tuple from a *fixed* argument list; what is
missing is its dynamic-length counterpart.

Estimated properly this time: a new primitive plus its FA constraint and
codegen, not a library change. The correct next step is to price that,
not to keep trying library formulations.