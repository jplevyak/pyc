# Issue 019: `dict` has no `__str__`/`__repr__`; `print(d)` shows `<instance>`

**Status:** fixed.
**Affects:** `__pyc__/07_dict.py` (`class dict` has no `__str__`/
`__repr__`, so it falls back to `__pyc_any_type__`'s generic
`__str__`, which just calls the `__pyc_to_str__` primitive and
prints a placeholder for any class without its own string
conversion).
**Related:** discovered while writing regression tests for issue 009
(dict comprehensions) — confirmed unrelated to comprehensions;
reproduces with a plain flat dict literal and zero comprehension
code involved. `set` (`__pyc__/08_set.py`, issue 008) does *not*
have this gap — it already defines `__str__` (modeled after real
Python's `{1, 2, 3}` repr).

## Symptom

```python
d = {"a": 1, "b": 2}
print(d)
```

Expected (CPython): `{'a': 1, 'b': 2}`.
Actual (pyc): `<instance>`.

## Root cause

`class dict` in `__pyc__/07_dict.py` defines `__len__`,
`__getitem__`, `__setitem__`, `get`, `update`, `__iter__`,
`__contains__`, `__pyc_to_bool__` — but no `__str__`/`__repr__`.
`print()`'s lowering (`python_ifa_build_if1.cc`, the `sym_print`
branch of `build_builtin_call_pyda`) calls `.__str__()` on every
argument via `call_method`; since `dict` doesn't override it, method
resolution falls back to `__pyc_any_type__.__str__`
(`__pyc__/00_runtime.py`), which just invokes the generic
`__pyc_to_str__` primitive — the same fallback any plain object
without a custom `__str__` gets, hence the generic `<instance>`
placeholder.

## What landed

Added `__str__` (and `__repr__`, aliasing to it like `set` does) to
`class dict`, modeled on `set.__str__` (`__pyc__/08_set.py`) and
`list.__str__` (`__pyc__/04_sequence.py`) — exactly per the fix
sketch:

```python
def __str__(self):
  x = "{"
  i = 0
  while i < self._len:
    if i:
      x += ", "
    x += self._keys[i].__repr__()
    x += ": "
    x += self._vals[i].__repr__()
    i += 1
  x += "}"
  return x
def __repr__(self):
  return self.__str__()
```

Verified against real `python3` output (`tests/dict_str.py`):
`print({"a": 1, "b": 2})` → `{'a': 1, 'b': 2}`; `print({})` → `{}`;
a second dict instance mutated after the first (exercising issue
017's now-fixed multi-instance path together with the new `__str__`)
prints correctly too. Passes on both the C and v2 LLVM backends.
Full suite 113/0 (was 112/0).

`str(d)` (the builtin *function* call, as opposed to the `.__str__()`
*method* used by `print()`/f-strings) still doesn't work — but that's
a separate, pre-existing gap affecting `str(x)` for **any** type, not
specific to `dict`, confirmed with a plain `str(5)` failing
identically. Filed separately as issue 020.

## Verification plan

1. `print({"a": 1, "b": 2})` prints `{'a': 1, 'b': 2}` (matching
   CPython's quoting via `str.__repr__`). ✓
2. `print({})` prints `{}`. ✓
3. Existing dict tests continue to pass unchanged. ✓
4. `tests/dict_str.py` + `.exec.check` added. ✓

## What this unblocks

`print(some_dict)` / debugging dict contents is an extremely common
operation, and now shows real contents instead of a useless
placeholder.
