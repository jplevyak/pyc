# 166 — strided slice assignment ignores the step and truncates the list

**Status: OPEN.** Pre-existing. Root-caused to a 3-line repro; this is
`sieve`'s wrong answer.

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

## Root cause (not yet located)

Unknown; the extended-slice **read** path is correct
(`_CG_list_getslice_internal` implements CPython's `PySlice_GetIndicesEx`
including negative steps, per its own comment), so the defect is on the
STORE side — either the frontend lowers `a[i::k] = v` to the two-argument
setslice, dropping the step, or the runtime setslice has no strided form.
Start at `__pyc__/04_sequence.py`'s `__setslice__`/`__setitem__` for a
slice argument and at `python_ifa_build_if1.cc`'s subscript-store
lowering.

## Verification plan

- The repro above, plus a negative step (`a[::-2] = ...`) and the
  CPython error case: an extended-slice assignment whose value length does
  not match the slice length must raise `ValueError`, where a contiguous
  slice assignment legitimately resizes.
- `sieve` must print `nprimes: 664579` for both sieves.
- Corpus: no regression; `sieve` moves out of stdout-differs once the
  wall-clock line is handled (see the note in 164 about lines CPython
  reports stably).
