# 125 — `in` dispatches `__contains__` directly, with no iterable fallback

**Status:** open. `range` fixed 2026-09-12; the general defect stands.

Found working the `unresolved call` class
([ifa/149](../ifa/issues/149-the-largest-diagnostic-class-reports-nothing.md)),
which is the largest warning class in the corpus. This was root cause #2 in
it, behind [ifa/150](../ifa/issues/150-is-not-none-never-folds.md).

## The defect

`emit_in_pyda` (`python_ifa_build_if1.cc:215`) lowers membership to exactly
one send on the right operand:

```c
// x in y      ->  y.__contains__(x)
// x not in y  ->  __not__(y.__contains__(x))
```

CPython does not. `x in y` tries `type(y).__contains__`; failing that it
falls back to the **iterable protocol** (`__iter__`, comparing each element
with `==`), and failing that to `__getitem__` with integer indices. pyc has
only the first step, so **any class that is iterable but does not define
`__contains__` cannot be membership-tested at all** — the send resolves to
nothing and the failure cascades as `expression has no type` through the
whole enclosing function.

## It has been plugged by hand three times

Each of these is the same gap, patched per class, with a comment saying so:

| class | file | comment says |
| --- | --- | --- |
| `__pyc_generator__` | `09_generator.py:112` | *"Even `3 in gen()` over plain ints failed ('illegal call argument type')"* |
| `__pyc_iterator__` | `00_runtime.py:222` | *"without this a bridged class can't be membership-tested at all"* |
| `__dict_iter__` | `07_dict.py` | (added for the same reason) |

All three are the identical consuming loop:

```python
while self.__pyc_more__():
  if self.__next__() == item:
    return True
return False
```

`range` was the fourth instance and was missed. Two lines reproduced it:

```python
x = 5
print(x in range(1, 10))
```

and it could not be served by either bridge — the `__pyc_iterator__` base is
only injected for **non-builtin** classes
(`python_ifa_build_syms.cc:1377`, *"Builtin classes are exempt"*), and
`range` is a builtin.

## `range` is fixed, arithmetically and deliberately not by a scan

`range.__contains__` (`05_builtins.py`) is CPython's O(1) arithmetic test,
for two independent reasons:

1. **It is what CPython does.** `range.__contains__` is O(1) for an int,
   which is why `10**9 - 1 in range(10**9)` returns immediately.
2. **pyc's range is its own iterator** — `__iter__` returns `this` and
   `__next__` mutates `self.i` — so the consuming loop the other three use
   would leave the range exhausted. `r = range(10); x in r; for i in r:`
   would silently iterate nothing. CPython's range is re-iterable; that
   deviation is pre-existing, and a scan here would have turned a missing
   method into a **wrong answer**.

The modulo is written over two non-negative operands rather than as
`(x - self.i) % self.s`, which for a negative step relies on Python's
sign-of-divisor modulo and would not survive lowering to C's.

`tests/in_range.py` pins it: 31 cases over positive steps, steps > 1,
negative steps, empty ranges, both boundaries, the one- and two-argument
forms, `not in`, and — the case that justifies the arithmetic — `3 in r`
followed by `for i in r` still summing 10. Output is byte-identical to
CPython and the compile golden is empty, so the test also asserts zero
warnings.

## Measured

Six gates green, **316/0** on both backends (the new fixture).

Corpus, `compile__default__ae80a6ed+7a8b764e` -> `2c1a8e8f+71bec4dd`. Only
the two affected programs move, and both go **fully clean**:

```
othello   6 -> 0
sudoku3  20 -> 0
warning lines   1382 -> 1356      programs warning  40 -> 38
```

The `check` sweep is the better result, because both programs were
previously in `run-fail` — they compiled with warnings and then ABORTED.
Joining per-program against `check__default__ae80a6ed+240f6fe9`, exactly two
rows change and no others:

```
othello   run 134 -> 0
sudoku3   run 134 -> 0
run_fail  38 -> 36
```

Both now run to completion, and their output is byte-identical to CPython
**except for a self-reported wall-clock line** they print themselves
(`TIME 15.00` against CPython's `TIME 16.71`; othello's other 7330 lines
match exactly). That is why `stdout_differs` goes 24 -> 26 while these two
are in fact correct — they simply reached the stdout comparison for the
first time.

## What is still open

The **fallback**, which is the actual defect. A user class today:

```python
class Bag:
    def __init__(self): self.v = [1, 2, 3]
    def __iter__(self): return iter(self.v)

print(2 in Bag())        # CPython: True.  pyc: unresolved dispatch
```

Three candidate shapes, in increasing order of fidelity:

- **`object.__contains__` as a base-class default** doing the iterable
  scan. Cheap, and there is precedent: `object.__not__` exists for exactly
  this reason (`00_runtime.py:154` — *"without this fallback a user object
  ... has no receiver and FA reports 'expression has no type'"*). The
  hazard is that it would silently apply the consuming loop to any
  self-iterator, which is the trap `range` just demonstrated — so it must
  not be added without deciding what a self-iterator should do.
- **Lower the fallback in the frontend**, emitting the scan when the
  receiver has `__iter__` and no `__contains__`. More faithful, but the
  lowering is static and the receiver's type is not known there, so it
  needs to be a post-FA decision or to emit both and let dispatch choose.
- **Also fall back to `__getitem__`**, CPython's third step. Only worth it
  if a corpus program needs it; none currently does.

Until one lands, every new iterable-without-`__contains__` class is a
silent cascade, and the only remedy is a fourth hand-written method.
