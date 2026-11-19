# 178 — `dict(m)` of a mapping reads it as an iterable of pairs

**Status:** CLOSED 2026-10-16. Found the same day while checking the README
against the tree. It was a silent wrong answer on both backends, in every mode.

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
([179](../179-sequence-index-out-of-range-does-not-raise.md)).

## Fix

Decide by dispatch, not in the frontend: send `x.__pyc_todict__(d)` (or
similar), with `dict` and `defaultdict` copying their items and a default
on `__pyc_any_type__` keeping the iterable-of-pairs loop. That also makes
`dict(**kw)` and `dict(x, k=v)` a separate, later question.

## Verification

`tests/dict_of_mapping.py` (`.known_issue`, CPython's output in its
`.exec.check`) flips to PASS on both backends.

## Resolution (2026-10-16)

As proposed, the choice is made by dispatch on `x`, which is how CPython
makes it (whether `x` has `keys`):

- `dict(x)` still lowers to `__pyc_dict_from_iterable__(d, x)`
  (`__pyc__/07_dict.py`), which is now `x.__pyc_dict_into__(d)`.
- `__pyc_any_type__.__pyc_dict_into__` (`00_runtime.py`) is the
  iterable-of-pairs loop, for lists, generators, `zip` results and the rest.
- `dict.__pyc_dict_into__` copies through `__pyc_dict_from_mapping__`
  (`d[k] = m[k]` for each `k in m.keys()`).
- Every class whose body defines `keys` gets the same method synthesized
  (`synthesize_dict_into`, `python_ifa_build_syms.cc`, beside
  `synthesize_default_iop`). That covers `defaultdict` (`pyc_lib`) and a
  user's own mapping with no special case. The name `keys` is CPython's
  protocol here, not an inference about the program.
- `dict.update(other)` had the mirror-image bug (it assumed a mapping, so
  `update([("t", 6)])` was wrong). It now dispatches the same way.

`tests/dict_of_mapping.py` covers a dict, a defaultdict, a user mapping,
three pairs forms (a list, a generator, `zip`) and `update` from each kind.
It is byte-identical to CPython on both backends and under `--strict`, and
its `.known_issue` is removed. `tests/dict_empty_next_to_populated.py` stays
KNOWN for ifa/072, as before. The empty `dict([])` beside a populated
int-keyed one still fails, now at the dispatch rather than in the old loop;
its sidecar is updated. `make test`: 475 passed, 17 known, 0 failed on both
backends. Sweep `check__default__7d425388+c45e758b` (`-R`): every corpus
compile, warning and stdout verdict is unchanged. EntrySets -1 and
CreationSets +244, a +3 floor on 65 programs from the new library code
(rdb +20).
