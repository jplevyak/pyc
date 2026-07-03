# Issue 019: `dict` has no `__str__`/`__repr__`; `print(d)` shows `<instance>`

**Status:** open.
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

## Proposed fix sketch

Add `__str__` (and `__repr__`, aliasing to it like `set` does) to
`class dict`, modeled on `set.__str__` (`__pyc__/08_set.py`) and
`list.__str__` (`__pyc__/04_sequence.py`):

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

## Verification plan

1. `print({"a": 1, "b": 2})` prints `{'a': 1, 'b': 2}` (matching
   CPython's quoting via `str.__repr__`).
2. `print({})` prints `{}`.
3. `str(d)` / f-string interpolation of a dict (`f"{d}"`, once issue
   006's format-spec-free path is exercised) produce the same text.
4. Existing dict tests continue to pass unchanged.

## What this unblocks

`print(some_dict)` / debugging dict contents is an extremely common
operation; today it silently produces a useless placeholder instead
of either the real contents or a clean error.
