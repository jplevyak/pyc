# Issue 017: A second instance of a mutating class silently reads/writes the wrong storage

**Status:** fixed.
**Affects:** `__pyc__/07_dict.py` (`dict`), `__pyc__/08_set.py` (`set`)
— both lacked an explicit `__init__`.
**Related:** discovered while implementing `issues/008` (set
literals/comprehensions) — `set`'s implementation
(`__pyc__/08_set.py`) follows the same pattern `dict` already uses,
and testing it multi-instance surfaced this pre-existing bug, which
turns out to affect `dict` identically with **zero** of the new set
code involved.

## Symptom

Minimal repro, using only the pre-existing `dict` builtin (no set
code involved at all):

```python
a = {"x": 1}
b = {}
b["y"] = 2
print(b["y"])
```

Expected (CPython): `2`.
Actual (pyc, before this fix, **both** C and v2 LLVM backends): `1`
— `b["y"]` read back `a`'s value for `"x"` instead of the value just
written to `b`. No crash, no diagnostic — a silent wrong-value bug.

## Root cause (traced)

`dict` (and the new `set`) declare their storage fields as bare
class-body attributes with no `__init__`:

```python
class dict:
  _keys = []
  _vals = []
  _len = 0
  def __setitem__(self, key, value):
    ...
    self._keys = self._keys.append(key)
    ...
```

This is a textbook instance of Python's own "mutable class attribute"
footgun — `_keys = []` at class-body level creates **one** list
object as a class attribute, shared by every instance until an
instance-level assignment overrides it. In real CPython, that
override would need to happen via something like
`self._keys = self._keys + [key]` (build a genuinely new list);
`list.append()` itself mutates in place and returns `None`.

pyc's own runtime deviates from that on purpose: `list.append()`
(`__pyc__/04_sequence.py`) returns the resized list rather than
`None`, specifically so that `self._keys = self._keys.append(key)`
*can* serve as the "break away from the shared default" idiom. But
tracing the generated C (confirmed with `fprintf`-based pointer/field
instrumentation on the minimal repro) showed `_CG_list_resize_internal`
(`pyc_c_runtime.h`, backing `.append()`) does **not** actually give
the caller an independent object — it grows the *input* list's data
buffer and rewrites that list's own header (`len`, `total_len`,
`ptr`) in place, then returns the very same header pointer. Since
`dict`'s prototype-based instantiation model (`gen_class_pyda` in
`python_ifa_build_syms.cc`) runs the class-body attribute
initialization (`___init___`, triple-underscore) **once**, on a
single shared prototype, and every `dict()` call clones that already-
initialized prototype by a shallow memberwise struct copy
(`_CG_prim_clone_dst`), every dict instance's `_keys`/`_vals` fields
start out holding the *same* pointer to the *same* shared list header
— exactly like the classic Python gotcha. `a`'s first `__setitem__`
call grows that shared header in place (still visible through `b`,
which was cloned from the same untouched prototype and holds the
identical pointer); `b`'s own subsequent append then computes its
"current length" from that already-grown shared header, appending
past data that's actually `a`'s.

I initially suspected a deeper FA/clone-level CreationSet-merging bug
(see the "reproduces on both backends" reasoning below) and prototyped
a fix in `pyc_c_runtime.h` making `_CG_list_resize_internal` /
`_CG_list_add_internal` / `_CG_list_setslice_internal` always allocate
a fresh header. That fixed the minimal repro but broke 5 existing
tests (`default_args.py`, `list_slicing.py`, `list_comprehension.py`,
both `fibheap_*` tests) — all of which correctly rely on
`list.append()` mutating in place and that mutation being visible
through aliases (`c = a; a.append(x)` making `c` see the change) or
across calls (Python's mutable-default-argument idiom). `list`'s
in-place-mutate-and-alias semantics are the *correct*, load-bearing
behavior elsewhere; the bug is narrower than "list mutation is
broken" — it's specifically that `dict`/`set` never gave each
instance its own storage in the first place.

(Reproducing on both the C and v2 LLVM backends is still explained
correctly by this cause: `_CG_list_resize_internal`'s in-place-header
mutation and `_CG_prim_clone_dst`'s shallow copy are both shared
runtime/frontend-level behavior, not backend-specific codegen, so a
frontend-level fix — giving each instance real per-instance
initialization — fixes both backends identically without touching
either emitter.)

## Fix

Added an explicit `__init__` to both classes, so every instance gets
its own fresh list objects via `_CG_prim_list(...)` at construction
time (`__new__()` already calls `__init__` once per cloned instance,
per `gen_class_pyda`'s existing "clone prototype, then call
`__init__`" flow — this is exactly the pattern `__dict_iter__`'s own
`__init__` in the same file already uses, immediately overwriting its
own bare class-body defaults):

```python
class dict:
  _keys = []
  _vals = []
  _len = 0
  def __init__(self):
    self._keys = []
    self._vals = []
    self._len = 0
  ...
```

and equivalently for `set`'s `_items`/`_len` in `__pyc__/08_set.py`.
No changes to `pyc_c_runtime.h`, `list.append()`, or any frontend
codegen were needed or kept — the (reverted) runtime-level attempt
above is documented here as a rejected approach, not the fix.

Verified: the minimal `dict` repro prints `2`; all four "narrowing
down the trigger" scenarios from the original symptom report now
produce correct output; an interleaved stress test with 4 concurrent
instances (2 dicts + 2 sets, mutations interleaved across all four)
matches `python3` byte-for-byte. Full suite 111/0 on both backends —
consolidated `tests/set_literal_basic.py` back into a single file now
that instances no longer need to be kept artificially isolated (see
issue 008's updated test list).

## What this unblocked

Any program constructing more than one `dict`/`set` instance and
mutating more than one of them was at risk of silent data corruption
— about as fundamental as correctness bugs get. This is now fixed for
the two affected builtins. User-defined classes with the same
bare-class-body-mutable-attribute-and-no-`__init__` shape have the
*same* footgun, but that's expected — it's the identical hazard real
Python has for that anti-pattern, not a pyc bug; `dict`/`set` just
shouldn't have shipped with it since users reasonably don't expect
builtin container types to exhibit it.
