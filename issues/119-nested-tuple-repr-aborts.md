# 119 — printing a NESTED tuple aborts at runtime

**Status:** open, filed 2026-08-29 while re-verifying
[ifa/061](../ifa/issues/061-CGEN-multi-tuple-list-null-element-type.md).
Split out because it is simpler than 061 and independent of it: no list,
no `sort()`, no heterogeneity.
**Affects:** `__pyc__/04_sequence.py`'s `tuple.__str__`/`__repr__` and
whatever resolves the per-element `__repr__` dispatch inside it.
**Reproducer:** `tests/nested_tuple_repr.py`, one line.

## Symptom

```python
print((1, (2, 3)))
```

compiles with **zero diagnostics** and then aborts:

```
_CG_string _CG_f_176_14(_CG_any): Assertion
  `!"runtime error: matching function not found"' failed.
```

CPython prints `(1, (2, 3))`.

The failing function returns `_CG_string` and takes `_CG_any`, i.e. the
`__repr__`/`__str__` dispatch on a tuple element whose type is the
`any` type — the element union of a tuple that holds both an `int64` and
another `tuple`.

## What is and is not affected

| program | result |
|---|---|
| `print((1, 2))` | fine |
| `print([(1, 2)])` | fine — `[(1, 2)]` |
| `print((1, (2, 3)))` | **aborts** |
| `print([(1, (2, 3))])` | **aborts** (same assertion) |

So it is specifically an element that is itself a tuple, sitting
alongside a scalar element. A tuple's elements are stored as its record
fields, so `(1, (2, 3))` has one `int64` field and one tuple field;
`__str__`'s loop dispatches `__repr__` per element and the dispatch has
no single resolution.

## Why this is not ifa/061

061 is about a LIST of tuples whose element type is contaminated by an
unrelated list's `.sort()` clone, and it fails at C **compile** time with
`incompatible pointer types`. This one has no list, no sort, and no
compile-time complaint at all — it is a runtime dispatch failure in the
builtin `__str__`. They were found together only because 061's repro
happens to print a list of nested tuples.

## Severity

Silent: zero warnings, exit 0, then an abort. That is the worst shape
(see [ifa/102](../ifa/issues/102-corpus-programs-compile-then-abort-at-runtime.md)),
and `print` of a nested tuple is an entirely ordinary thing to write.

## Verification plan

- `tests/nested_tuple_repr.py` prints `(1, (2, 3))` and matches CPython;
  drop its `.known_issue`.
- `print([(1, (2, 3))])` also works — same root, one level out.
- No change to `print((1, 2))` / `print([(1, 2)])`, which are fine today.
