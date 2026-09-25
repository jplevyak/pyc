# 170 — a file object is not a context manager: `with open(...) as f` does not compile

**Status:** CLOSED — fixed 2026-09-25, the day it was filed while testing
the [106](106-empty-if-body-silently-accepted.md) parser fix.

Test: `tests/with_open_file.py`.

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

`__pyc_file__` in [`__pyc__/07_file.py`](../../__pyc__/07_file.py) has no
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

## Resolution (2026-09-25)

**1. The methods.** `__pyc_file__` and `__pyc_binfile__` in
`__pyc__/07_file.py` now have `__enter__` (returns `self`) and
`__exit__(typ, value, tb)` (closes the file, returns `False`), as
CPython's file objects do.

**2. The diagnostic, which was not specific to `with`.** A missing member
never had a message of its own anywhere. `x.foo()` on a class with no
`foo` reported only `expression has no type`. The analysis lowers `o.m`
to a `P_prim_period`, and when no CreationSet of `o` has `m` the result
is left bottom. `ATypeViolation_kind::MEMBER` had a printer in
`fa_debug.cc` but nothing ever raised it. `collect_member_violations`
(`fa.cc`) now raises it on the converged types, for each reachable period
lookup whose receiver is typed but whose result is still bottom:

```
d.py:4:16: error: unresolved member 'foo' of class 'C'
    x.foo()
           ^
```

`with C():` on a class without the protocol now reports
`unresolved member '__enter__' of class 'C'` (and `'__exit__'`), and a
function called on two classes gets one line per class. A union receiver
with the member on only SOME of its classes has a typed result and is
not reported, so the new check adds a precise message where compilation
already failed but does not reject anything new.

**Corpus.** `./corpus_sweep.sh -m check`
(`sweeps/check__default__89a52795+df0e1c35.tsv`) against
`check__default__5caf8dc2`: all 77 programs have identical compile, run
and stdout verdicts, so compile_fail stays at 32. `neural1` loses the 5
errors at its `with open('jets.txt','wb') as fh:` (lines 154-155,
`'fh' has no type` among them) and still fails on the other 9, all at
lines 56-57: `sorted(..., reverse=True)` and `enumerate` over its
result, which are unrelated to this issue.

**Test.** `tests/with_open_file.py` covers text and binary modes, a
`with` inside a method, and an exception raised in the body. That the
data written just before the raise reads back shows `__exit__` closed
(and flushed) the file on the exception path. Printing the bytes directly
would hit [051](../051-bytes-repr-does-not-escape.md), so the test
compares them.

## Verification plan (original)

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
