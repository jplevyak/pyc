# 170 — a file object is not a context manager: `with open(...) as f` does not compile

**Status:** open, root-caused. Found 2026-09-25 while testing the
[106](closed/106-empty-if-body-silently-accepted.md) parser fix.

Repro: `tests/with_open_file.py` (`.known_issue`).

## Symptom

```python
with open(__file__) as f:
    print(len(f.read()) > 0)
```

| | result |
|---|---|
| CPython | `True` |
| **pyc** | `error: 'f' has no type` — and, inside a function or method, `illegal call argument type 'None' illegal: __pyc_None_type__` at the `with` |

Opening and closing without `with` works (`f = open(p); f.close()`).

## Root cause

`__pyc_file__` in [`__pyc__/07_file.py`](../__pyc__/07_file.py) has no
`__enter__` or `__exit__`. The `with` lowering
(`python_ifa_build_if1.cc`, around the `call_method(... "__enter__" ...)`
at line 2742) calls both on the context expression, so on a file both calls
are unresolved and `f` is left untyped.

## Two defects

1. **The missing methods.** CPython's file `__enter__` returns the file
   itself, and `__exit__` closes it and returns a false value, so an
   exception propagates. Both are a few lines on `__pyc_file__` and on
   `__pyc_binfile__`, the separate class `open_binary` returns.
2. **The diagnostic.** An unresolved `__enter__` is reported as an
   untyped `f`, or as a `None` argument, neither of which names the
   missing method. A `with` on an object that has no `__enter__` should
   say so, as `unresolved call '__gt__'` does for operators.

## Verification plan

- `tests/with_open_file.py`, which writes a file with `with`, reads it back
  with `with`, and checks that the file is closed after the block, matches
  CPython; delete its `.known_issue`.
- The same inside a method.
- `make test` green, and `./corpus_sweep.sh -m check` for `neural1`,
  the one corpus program that uses `with open(...)`.

## What this unblocks

`with open(...) as f:` is the idiomatic way to read or write a file in
Python 3, so every program that does file I/O that way, including the
corpus program `neural1`.
