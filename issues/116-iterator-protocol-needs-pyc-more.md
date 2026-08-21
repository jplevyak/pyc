# 116 — a class with `__iter__`/`__next__` iterates zero times, silently

**Status:** open, found 2026-08-21 while clearing issues/115's residue.
`pyc_lib/itertools.py`'s `count` is fixed; the general gap is not.

## Symptom

```python
class Counter:
    def __init__(self): self.n = 0
    def __iter__(self): return self
    def __next__(self):
        self.n += 1
        return self.n

for x in Counter():
    if x > 3: break
    print(x)
```

    CPython:  1 2 3          pyc:  (no output, exit 0)

No warning, no error, exit 0. The class implements the standard Python
iterator protocol exactly and the loop body simply never runs.

## Cause

pyc's for-loop is **peek-then-fetch**, not fetch-until-StopIteration.
`PY_for_stmt` (python_ifa_build_if1.cc) lowers to

    iter = obj.__iter__()
    while iter.__pyc_more__():
        x = iter.__next__()

and `object.__pyc_more__` (`__pyc__/00_runtime.py`) returns **False**:

```python
class object:
    def __null__(self):
        return False
    def __pyc_more__(self):
        return False
```

So any class that doesn't override `__pyc_more__` is reported empty.
Every iterator in `__pyc__/` defines one, which is why nothing in the
suite caught it — but `__pyc_more__` is not a Python method, so no user
code and no ported library will ever define it.

Two protocols with different shapes:

| | pyc | CPython |
|---|---|---|
| more? | `__pyc_more__()` → bool | (no equivalent) |
| next | `__next__()` | `__next__()`, raises StopIteration |

Peek-then-fetch is the cheaper shape for a compiler — no exception on
the hot path — and `__pyc_generator__` shows the bridge is buildable:
it advances eagerly and remembers the value in `nextval`/`primed`.

## Fix

Bridge the two in the for-loop lowering, not on `object`: when the
receiver defines `__next__` but no `__pyc_more__`, lower the loop to
CPython's shape instead — call `__next__` inside a try, catch
StopIteration, break. issues/011's exception machinery already provides
everything needed, and generators/builtins keep the cheap path because
they do define `__pyc_more__`.

A default on `object` cannot work: bridging needs somewhere to stash the
peeked value, and a base-class method has no per-instance state to use
without adding fields to every object in the program.

Whatever the shape, **`object.__pyc_more__` returning False must go**.
Silently reporting a non-empty iterator as empty is the worst available
answer; if the protocol can't be satisfied, the call should not resolve.

## What this blocks

`pyc_lib/itertools.py`'s `count` was the live instance — `for j in
count(...)` yielded nothing, so `shedskin_examples/sunfish`'s
`gen_moves` scanned every ray as empty. Fixed directly there (a `count`
is infinite, so its `__pyc_more__` is `return True`), covered by
`tests/itertools_count_forloop.py`. It was the only such class in
`pyc_lib/` or `__pyc__/`, but it is exactly the shape any ported
library or user class will have.

## Verification plan

- The repro above prints `1` / `2` / `3`.
- A finite `__iter__`/`__next__` class raising StopIteration terminates
  the loop at the right point.
- `list(obj)`, `in`, and comprehensions over such a class agree with
  CPython.
- Generators and builtin containers keep the `__pyc_more__` path (no
  new try/except in their emitted code).
- `tests/itertools_count_forloop.py` still passes.
