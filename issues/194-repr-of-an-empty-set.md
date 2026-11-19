# 194 — an empty set prints as `{}`

**Status:** open, filed 2026-11-19. Found while scoping dataclasses (a
`default_factory=set` field is usually empty). A **silent wrong answer**:
`{}` is CPython's spelling of an empty DICT.

## Symptom

```python
s = set()
print(s, repr(s), str(s))     # CPython: set() set() set()    pyc: {} {} {}
print("%r" % (s,), f"{s!r}")  # CPython: set() set()          pyc: {} {}
print([s], (s, 1))            # CPython: [set()] (set(), 1)   pyc: [{}] ({}, 1)
t = {1}; t.remove(1); print(t)  # CPython: set()             pyc: {}
```

Both backends, every mode. A non-empty set prints correctly (`{2}`).

## Cause

`set.__str__` (`__pyc__/08_set.py`) writes `{`, the items, `}` with no
case for zero items, and every route above (`repr`, `str`, `print`, `%r`,
f-strings, a container's repr) reaches it.

## Fix

Return `"set()"` when `self._len` is 0. Test: the lines above in
`tests/`, with an `.exec.check` from CPython.

Related, not this issue: `frozenset` is not supported at all ("builtin
'frozenset' is not supported by pyc").
