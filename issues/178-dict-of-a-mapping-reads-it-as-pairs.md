# 178 — `dict(m)` of a mapping reads it as an iterable of pairs

**Status:** open. Found 2026-10-16 while checking the README against the
tree. Silent wrong answer, both backends, every mode.

## Symptom

```python
d = {"ab": 1, "cd": 2}
print(dict(d))          # CPython: {'ab': 1, 'cd': 2}.   pyc: {'a': 'b', 'c': 'd'}

from collections import defaultdict
dd = defaultdict(int)
dd["a"] += 1
print(dict(dd))         # CPython: {'a': 1}.   pyc: {'a': '\x00'}
```

`dd["a"]` and `sorted(dd.items())` are right. Only the conversion is wrong.

## Root cause

`dict(x)` with one positional argument is lowered in the frontend
(`python_ifa_build_if1.cc`, the `"dict"` constructor intercept) to
`__pyc_dict_from_iterable__(d, x)` (`__pyc__/07_dict.py`), which does
`d[pair[0]] = pair[1]` for each `pair in x`. CPython's `dict(x)` takes that
path only when `x` has no `keys()`. For a mapping it copies `x[k]` for each
`k in x.keys()`. Iterating a dict yields its KEYS, so each key string is read
as a pair: `"ab"` gives `("a", "b")`. The defaultdict's `"a"` gives
`("a", "a"[1])`, and `"a"[1]` reads the terminating NUL instead of raising
([179](179-sequence-index-out-of-range-does-not-raise.md)).

## Fix

Decide by dispatch, not in the frontend: send `x.__pyc_todict__(d)` (or
similar), with `dict` and `defaultdict` copying their items and a default
on `__pyc_any_type__` keeping the iterable-of-pairs loop. That also makes
`dict(**kw)` and `dict(x, k=v)` a separate, later question.

## Verification

`tests/dict_of_mapping.py` (`.known_issue`, CPython's output in its
`.exec.check`) flips to PASS on both backends.
