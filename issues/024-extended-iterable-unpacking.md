# Issue 024: Extended iterable unpacking (`a, *b = [1, 2, 3]`, PEP 3132)

**Status:** open.
**Affects:** `python.g` and pyc lowering.

## Description

```python
a, *b = [1, 2, 3]
```
```
star_unpack.py:1: syntax error after ','
```
No starred-target form in the assignment-target grammar (`expr_stmt`, `python.g:80`). Note this is *not* the same gap as `*args` in function definitions/calls, which already works (see `PY_star_arg`/`PY_dstar_arg` handling in `python_ifa_build_syms.cc`/`python_ifa_build_if1.cc`) — this is specifically the assignment-target position. Once parsed, the lowering needs to bind the starred name to a (list) slice of the remaining unpacked elements, alongside the existing plain multi-target unpacking already supported (`tests/tuple_unpack.py`).

## Verification plan
1. Extended unpacking: `a, *b = [1, 2, 3]` gives `a == 1`, `b == [2, 3]`; also the leading/middle-star forms (`*a, b = ...`, `a, *b, c = ...`).
2. Add one test file for extended unpacking once implemented.
