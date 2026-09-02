# 119 — printing a HETEROGENEOUS tuple aborts at runtime

**Status: CLOSED 2026-08-29 — fixed on BOTH backends.** `tuple.__str__`
and `tuple.__hash__` are now generated UNROLLED at the program's max
tuple arity by `inject_tuple_methods` (`python_ifa_main.cc`), joining
`__eq__`/`__lt__` which had been unrolled for this exact reason since
issue 069. `PYC_TUPLE_AS_LIST` defaults on to supply a valid layout for
the runtime-arity tuples that unrolling then exposes. Regression test:
`tests/nested_tuple_repr.py` (`.known_issue` dropped). See
**Resolution** at the bottom. Filed 2026-08-29 while re-verifying
[ifa/061](../../ifa/issues/061-CGEN-multi-tuple-list-null-element-type.md).
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
(see [ifa/102](../../ifa/issues/102-corpus-programs-compile-then-abort-at-runtime.md)),
and `print` of a nested tuple is an entirely ordinary thing to write.

## Verification plan

- `tests/nested_tuple_repr.py` prints `(1, (2, 3))` and matches CPython;
  drop its `.known_issue`.
- `print([(1, (2, 3))])` also works — same root, one level out.
- No change to `print((1, 2))` / `print([(1, 2)])`, which are fine today.

## Resolution (2026-08-29)

### The title was wrong: it is heterogeneity, not nesting

Nesting was incidental. The rule is that the element types DIFFER:

| program | before |
|---|---|
| `print(((1, 2), (3, 4)))` | fine — **homogeneous**, which hid the bug |
| `print((1, (2, 3)))` | aborted (int64 + tuple) |
| `print((1, [2]))` | aborted (int64 + list) |
| `print((1, "a"))` | **refused**: `mixed basic types: ( int64 str )` |

`tuple.__str__` was an index loop doing `self[k].__repr__()`. With a
runtime `k`, `self[k]` is the union of every field type, so the
per-element `__repr__` dispatch had no single resolution — C aborted
with `matching function not found`, LLVM silently printed `(, )`. Where
the union mixed WIDTHS it never got that far: the BOXING check refused
the program outright, which is why that case looked like issues/018 and
not like this one.

`tuple.__hash__` had the identical loop and the identical bug, beneath a
comment asserting the loop was safe there because the result type is
`int` on every branch. It is the DISPATCH that fails, not the result
type, so `hash((1, (2, 3)))` aborted too.

### The fix

A CONSTANT index names ONE field, so every dispatch resolves and no
element union is ever formed — which is also why the mixed-width case
now compiles: there is nothing left to box. `__contains__` was already
fine because it routes through the already-unrolled `__eq__`.

### The second half: unrolling exposed an empty-record layout

A sliced tuple has RUNTIME arity, so none of the unrolled `n >= k`
guards fold and the constant-index path stays live — on a CreationSet
`clone.cc` had given RECORD layout with ZERO members, whose `c_type()`
is `_CG_void` and whose every getter is `runtime error: bad getter`
(the pathology `fa.cc:2273` and `clone.cc:734` both describe).

That defect was **already reachable without any of this**: `s[0]` and
`s == (1, 2)` on a slice both aborted before this change, since `__eq__`
was already unrolled. `tests/tuple_slice.py` passed only because it
exclusively *printed* slices, and the old looped `__str__` indexed with
a runtime `k`, which never takes the record getter path.

`PYC_TUPLE_AS_LIST` now defaults to 1. It does not make tuples lists —
`clone.cc:793`'s `!tup` still keeps every record-able tuple a record —
it only supplies a valid layout where the bogus empty record used to go.

### Verified

All five CI gates green. Both backends: 307 passed / 0 failed, known
issues 16 → 15. The one golden that moved is `minmax_3arg.py.check`,
which embeds `__pyc__.py` line numbers (issues/111); the diff is a
uniform −22 line shift and nothing else.

### What this also fixed

[ifa/061](../../ifa/issues/061-CGEN-multi-tuple-list-null-element-type.md)'s
LLVM half. Its live repro printed `[(, ), (, )]` and now prints
`[(2, (1, 9)), (1, (5, 5))]` — that symptom was this bug, not 061's.
061 stays open, now C-backend-only (`incompatible pointer types`).
