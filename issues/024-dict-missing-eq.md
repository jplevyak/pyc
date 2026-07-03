# Issue 024: `dict` has no `__eq__`/`__ne__`

**Status:** open.
**Affects:** `__pyc__/07_dict.py` (`class dict`).
**Related:** discovered while fixing
[023-tuple-missing-eq-str.md](closed/023-tuple-missing-eq-str.md) — checked
whether `list`/`tuple`'s `__eq__`/`__ne__` fix pattern
(`__pyc__/04_sequence.py`) should extend to `dict` too, and found
`dict` has neither method at all, unlike `set` (which already has
both, `__pyc__/08_set.py:69,76`).

## Symptom

```python
print({"a": 1, "b": 2} == {"a": 1, "b": 2})
```

```
warning: unresolved call '__eq__'
...: illegal call argument type expression illegal: dict
...: expression has no type
```

## Why this is bigger than list/tuple's fix

`list`/`tuple` equality is a straightforward index-aligned
element-by-element comparison (issue 023's fix). `dict` equality
needs to be **key-order-independent** — two dicts built by inserting
the same key/value pairs in different orders must compare equal
(`{"a": 1, "b": 2} == {"b": 2, "a": 1}` is `True` in CPython). A
naive port of `list`'s pattern (compare `_keys`/`_vals` arrays
index-for-index) would be wrong whenever insertion order differs,
which is common. The correct implementation needs something closer
to `set.__eq__`'s existing shape (check same length, then for each
key in `self`, look it up in `other` via `__contains__`/`__getitem__`
and compare values) — `set.__eq__` (`__pyc__/08_set.py:69`) is a
reasonable model for the length-check plus mutual-containment
structure, adapted to also compare values, not just membership.

## Proposed fix sketch

```python
def __eq__(self, d):
  if len(self) != len(d):
    return False
  for i in range(len(self)):
    k = self._keys[i]
    if not d.__contains__(k):
      return False
    if d[k] != self._vals[i]:
      return False
  return True
def __ne__(self, d):
  return not self.__eq__(d)
```

## Verification plan

1. `{"a": 1, "b": 2} == {"a": 1, "b": 2}` → `True`.
2. `{"a": 1, "b": 2} == {"b": 2, "a": 1}` → `True` (key-order
   independent — the case that would break a naive index-aligned
   port of list's fix).
3. `{"a": 1} == {"a": 2}` → `False`; `{"a": 1} == {"a": 1, "b": 2}`
   → `False`.
4. `!=` mirrors `==` throughout.
5. Existing dict tests continue to pass unchanged.

## What this unblocks

Comparing dicts (config equality checks, memoization/caching keyed
on dict contents, test assertions) is a common operation that
currently fails to compile for any dict.
