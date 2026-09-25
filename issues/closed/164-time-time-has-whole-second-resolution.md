# 164 — `time.time()` had whole-second resolution, so every self-timing program printed nonsense

**Status: FIXED** 2026-09-18 (`pyc_lib/time.py`).

**Related:** [165](165-percent-d-truncates-a-64-bit-int-to-32-bits.md) (found
in the same measurement), [163](163-stdout-check-counted-nondeterministic-lines.md)
(the sweep's variance filter, which was hiding some of this).

## Symptom

```python
import time
a = time.time()
... work ...
print("%.4f" % (time.time() - a))
```

pyc printed `0.0000` for a loop CPython measured at 1.2921 s. Three
corpus programs were visibly wrong because of it:

| program | pyc | CPython |
| --- | --- | --- |
| `sieve` | `time: 0.00` | `time: 0.85` |
| `tictactoe` | `TIME 3.00` | `TIME 0.96` |
| `pystone` | `This machine benchmarks at 1000000.000000 pystones/second` | a real rate |

`pystone` is the clearest: it divides its loop count by the elapsed time,
so a zero-or-one-second delta produces a flat, fabricated number rather
than a benchmark.

## Root cause

`pyc_lib/time.py` was

```python
def time():
    return float(__pyc_c_call__(int, "time", int, 0))
```

libc `time(0)` — whole seconds. The shim's own comment documented this as
a limitation ("sub-second timing is not modelled"), so it read as a known
gap rather than a bug.

It was not a gap: the runtime has always exported a microsecond clock.
`_CG_get_time()` (`pyc_runtime.c`, `gettimeofday`) was added for the async
timer queue and returns a `double`. `time.time()` simply never used it.

## Fix

```python
def time():
    return __pyc_c_call__(float, "_CG_get_time")
```

`_CG_get_time` is declared in `pyc_c_runtime.h` and defined in
`pyc_runtime.c`, which both backends link (`Makefile.cg` links
`pyc_runtime.o`), so this needed no new runtime code — the same pattern
`_CG_net_poll_read` already uses from `pyc_lib/select.py`.

## Measured

- The repro above reports a real sub-second delta on both backends
  (0.2953 C, 0.3455 LLVM, against CPython's 0.0334 — pyc is slower here,
  but it is now *measuring*).
- Six CI gates green, suite 315/0/26 both backends.

Note when reading a timing test on the LLVM backend: it folds a
closed-form loop (`s = s + i` over a range) away entirely, so `t1 - t0`
is legitimately ~0 there. Use a workload it cannot fold (string building)
to check the clock itself.
