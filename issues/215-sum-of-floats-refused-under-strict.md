# 215 — `sum()` of a float list is refused under `--strict`

**Status:** open. Found 2026-11-25 while writing the array test of
[214](214-struct-formats-and-array-float-typecodes.md); also at HEAD
before that change.

## Symptom

```python
print(sum([1.5, 2.0]))
```

prints `3.5` by default, with the permissive numeric widening. Under
`--strict` it is refused:

```
su.py:1:6: error: expression has mixed basic types:( int64 float64 )
```

## Mechanism

`sum` (`__pyc__/05_builtins.py`) starts from `t = start`, the int `0`, and
returns `t` when the loop never sees a float. A float list's contour folds
`isinstance(x, float)` true and returns through `__pyc_sum_floats__`, but
the `return t` after the loop is still reachable for an EMPTY list. In
CPython that path really does return the int `0` (`sum([])` is `0`), so the
result is a genuine `{int, float}` on paper: nothing proves the list
non-empty.

## Direction

Not a widening. The int arm is reachable only through the zero-iteration
exit of a loop over a float container. Candidates to measure:

- the loop-over-empty-container family ([ifa/072](../ifa/issues/072-FA-empty-container-notype-current-mechanism-and-plan.md)
  treats a loop over an empty container as live code); here the container
  is not empty, but the zero-trip exit is typed as if it might be;
- a per-call-site answer where the argument is a non-empty literal.

`--strict` must keep refusing a sum whose list may really be empty, since
CPython returns an int there.
