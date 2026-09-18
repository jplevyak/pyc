# 166 — strided slice assignment ignores the step and truncates the list

**Status: FIXED** 2026-09-18 (`__pyc__/04_sequence.py`
`__pyc_setslice__`, `pyc_c_runtime.h` `_CG_list_setslice_strided`,
`pyc_runtime.c` `_CG_list_setslice`). Fixes `sieve`. Regression test
`tests/strided_slice_assign.py`.

**One half remains open** — a strided `del` — see "Still open" below.

**Related:** [164](164-time-time-has-whole-second-resolution.md) (whose
measurement exposed this by un-masking `sieve`'s real diff),
[163](163-stdout-check-counted-nondeterministic-lines.md) (the variance
filter that decides what a corpus stdout difference means).

## Symptom

```python
a = list(range(20))
a[3::4] = [0, 0, 0, 0, 0]
print(a)
```

| | result |
| --- | --- |
| CPython | `[0, 1, 2, 0, 4, 5, 6, 0, 8, 9, 10, 0, 12, 13, 14, 0, 16, 17, 18, 0]` |
| pyc | `[0, 1, 2, 0, 0, 0, 0, 0]` |

pyc assigns the 5 values **contiguously** starting at index 3 — i.e. it
treats `a[3::4] = v` as `a[3:8] = v` — and the list is TRUNCATED from 20
elements to 8. The step is dropped entirely. Reproduces on both backends
and for every step tested (`b[2::3]`, `c[0::2]`).

## Why it matters

`shedskin_examples/sieve` is wrong because of exactly this. Its
Sieve of Eratosthenes is

```python
sieve = list(range(3, n, 2))
top = len(sieve)
for si in sieve:
    if si:
        bottom = (si*si - 3) // 2
        if bottom >= top:
            break
        sieve[bottom::si] = [0] * -((bottom - top) // si)
return [2] + [el for el in sieve if el]
```

so the strided store is the algorithm's core. pyc prints
`nprimes: 4` where CPython prints `nprimes: 664579`, with **no warning
and exit 0** — the program compiles clean and lies, which is the class
[ifa/158](../ifa/issues/158-FA-every-type-violation-is-fatal.md) exists to
stop and cannot catch because nothing here fails to type.

This was mis-attributed once, and the mistake is worth recording: `sieve`
was listed among the corpus programs whose stdout difference was the
whole-second `time.time()` of [164](164-time-time-has-whole-second-resolution.md).
Its `time:` line WAS quantised and 164 did fix that — but the `nprimes`
line was wrong the whole time and the timing line was the more eye-catching
half of the diff. Reading a diff's first hunk is not reading the diff.

## Root cause

**Both** of the suspected places, in sequence. The frontend is innocent —
`python_ifa_build_if1.cc` computes the real step and passes it as the
third argument of the `__pyc_setslice__` send, exactly as it does for
`__pyc_getslice__`. Then:

1. `__pyc__/04_sequence.py`'s `__pyc_setslice__(self, i, j, s, v)`
   accepted `s` and **dropped it** — its `__pyc_c_call__` passed only
   `(self, size, i, j, v)`. The sibling `__pyc_getslice__` two lines above
   has always passed its step, which is why reads were correct.
2. `_CG_list_setslice_internal` had **no step parameter to receive**, in
   either copy (the inline one in `pyc_c_runtime.h` for the C backend and
   the out-of-line one in `pyc_runtime.c` for LLVM). It implements only
   the contiguous splice: delete `[l, h)`, insert `l2`, resize.

So `a[3::4] = [0]*5` became `a[3:8] = [0]*5` — replace five elements and
resize 20 → 8.

## Fix

A strided branch in both runtime copies, reusing
`_CG_list_getslice_internal`'s normalisation verbatim so the read and
write sides cannot drift, plus the step threaded through
`__pyc_setslice__`.

The two branches differ deliberately, because CPython's do: a
**contiguous** slice store may resize (`a[1:3] = [9]` shortens the list),
but an **extended** one may not — the value length must equal the slice
length, or CPython raises `ValueError: attempt to assign sequence of size
N to extended slice of size M`. So `k == 1` keeps the existing
splice-and-resize path byte-for-byte and the strided path stores in place.

pyc has no exception path out of a runtime helper, so the length mismatch
takes `cg.cc`'s `assert(!"runtime error: ...")` convention — loud, rather
than the silent corruption it replaces. Matching CPython's `ValueError`
exactly is left to the exception work (issues/011's family).

## Measured

Eleven cases byte-identical to CPython on **both** backends: the three
repro forms, negative steps (`d[::-2]`, `e[8:2:-2]`), an explicit step 1
(contiguous, resizes), no step (contiguous, resizes), `del h[1:3]`
(contiguous, through the same helper), a strided READ pinned beside the
writes, and `sieve`'s Eratosthenes reduced to 30 and 1000.

`sieve` itself now prints `nprimes: 664579` for both sieves, against the
`nprimes: 4` it printed before.

Six CI gates green, suite 316 passed / 0 failed / 26 known on both
backends.

## Still open — a strided `del`

`del a[i:j:k]` with `k != 1` is a **different operation** from an extended
slice store: CPython removes the selected elements and shrinks the list,
where `a[::2] = []` is a `ValueError`. pyc's lowering cannot tell them
apart — `emit_del_target` lowers `del o[i:j]` to `o[i:j] = []`, so a
strided delete arrives at the runtime indistinguishable from a strided
store of an empty list.

`__pyc_delslice__` therefore passes a step of **1**, preserving today's
contiguous-delete behaviour exactly. A strided `del` is consequently still
wrong (it deletes contiguously), and no corpus program uses one — grep
finds zero `del ...[...::...]` across all 77.

Fixing it means routing `emit_del_target` through `__pyc_delslice__`
(which already exists and carries the right ifa/133 element-channel
constraint — a SELF-merge, not `merge_in(self, v)` — so the reroute would
also retire the latent element-channel bug its own comment describes) and
giving the runtime a `_CG_list_delslice` with the strided form.
