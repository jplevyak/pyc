# 123 — `str()`/`print()` do not fall back to a class's `__repr__`

**Status:** open, filed 2026-09-03. Found in the corpus `check` sweep
taken for [ifa/124](../ifa/issues/124-FA-refuse-imprecise-inference.md):
`go` began compiling and running for the first time, and its stdout
still did not match CPython — for this reason, unrelated to that fix.
**Area:** pyc builtin library (`__pyc__/00_runtime.py`), object protocol.
**Severity:** **silent** — zero warnings, exit 0, wrong output.
**Reproducer:** `tests/repr_without_str.py`, 5 lines.

## Symptom

```python
class C:
    def __repr__(self):
        return "hello"
print(C())
print(str(C()))
```

| | output |
|---|---|
| CPython | `hello` / `hello` |
| **pyc** | **`<object>` / `<object>`** — no warnings, exit 0 |

## Cause

`__pyc__/00_runtime.py`:

```python
class object:
  def __null__(self):
    return False
  def __str__(self):
    return "<object>"
```

A user class that defines `__repr__` and not `__str__` inherits this
`object.__str__`, and nothing consults `__repr__`. In CPython the two are
linked: `object.__str__` **calls `self.__repr__()`**, so defining
`__repr__` alone gives you both, and defining `__str__` alone leaves
`repr()` at the default. pyc implements them as unrelated methods.

Defining `__repr__` without `__str__` is the common case, not a corner —
it is what you write when you want one readable rendering.

## Why it surfaced now

`go` prints the board with `print(board)`, and `Board` (go.py:310)
defines `__repr__` only. Until ifa/124's splitter fix, `go` did not
compile at all, so the corpus sweep's `stdout_differs` column had never
had an opinion about it. It compiles and runs now, and the first 9 lines
of every board are `<object>` where CPython prints the position:

```
CPython                    pyc
. . . . . . . . .          <object>
. . . . . . . . .          thinking..
...                        I move here: (5, 3)
```

(`go`'s move choices also differ between the two, but that is unseeded
`random.randrange` and is not comparable — the `<object>` rendering is
the real defect.)

## Fix

Make `object.__str__` delegate:

```python
class object:
  def __str__(self):
    return self.__repr__()
  def __repr__(self):
    return "<object>"
```

so the default rendering lives in `__repr__` (where CPython keeps it) and
`__str__` inherits it. Two things to check while doing it:

- **Dispatch cost.** `__str__` becomes a polymorphic call into
  `__repr__` on every user object printed. Every class gets a
  `__repr__` slot whether or not it defines one, which is the
  member-slot growth [ifa/110](../ifa/issues/closed/110-override-duplicates-member-slot.md)
  was about — measure `ess`/`css` on the corpus, not just the suite.
- **The reverse direction is NOT symmetric.** A class defining `__str__`
  only must still get the DEFAULT `repr()`, not its `__str__`. Do not
  implement this as an alias.

## Verification plan

1. The repro above prints `hello` twice.
2. A class defining `__str__` only: `print(x)` uses it, and `repr(x)`
   does **not** — still the default rendering.
3. A class defining both: each is used in its own position.
4. `go`'s stdout matches CPython on the board lines (its move choices
   will not — unseeded `random`).
5. Corpus sweep: no new `stdout_differs`, and check `ess`/`css` for the
   extra slot.

## What this unblocks

`go`'s output correctness, and any corpus program that renders objects
this way. More generally it is a **silent** wrong-answer class: nothing
in the compile output hints at it, so `-m compile` and `-m run` sweeps
are both blind to it and only `-m check` sees it — the argument
[ifa/102](../ifa/issues/102-corpus-programs-compile-then-abort-at-runtime.md)
makes for running `check`.
