# 111 — COMPILE-OUT checks embed `__pyc__.py` line numbers

**Status:** open
**Area:** test harness

## Symptom

`tests/minmax_3arg.py.check` (a COMPILE-OUT check) records diagnostic
text that includes the *virtual* line numbers of the concatenated
builtin module:

```
minmax_3arg.py:16:185: warning: illegal call argument type 'c' illegal:
    print(min(2, 9))
                    ^
  called from __pyc__.py:1605
```

`__pyc__.py` is synthesized in memory from `__pyc__/*.py`, so **any**
edit to an earlier builtin file shifts every subsequent line number and
the check stops matching. Adding an 11-line method to `class tuple` in
`04_sequence.py` moved `1605` to `1616` and the test "failed" with no
behavioural change whatsoever.

## Why it matters

This is not just noise — it produces confidently wrong conclusions.
During issues/110 it was read twice as a real regression, and led to a
recorded (and now retracted) claim that adding any method to
`class tuple` merges every tuple contour and degrades FA precision.
The actual diff was line numbers and nothing else:

```
$ diff actual minmax_3arg.py.check | grep '^[<>]' \
    | grep -v 'called from __pyc__.py:[0-9]*'
(empty)
```

Every builtin-library change now carries a false-failure tax, and the
natural response — regenerate the check — silently accepts whatever
else may have changed in it.

## Proposed fix

Normalize `__pyc__.py:<N>` to `__pyc__.py:N` when comparing COMPILE-OUT
output against a check, the way the sweep's failure buckets already
normalize digits. A user-program line number is meaningful and must
stay exact; a builtin-module line number is an implementation detail of
how the library files happen to be concatenated.

Alternatively, report builtin frames as `<file>/<line>` against the
actual `__pyc__/NN_*.py` source, which is more useful to read and is
stable under edits to *other* library files.

## Verification

- `make test-e2e` clean before and after.
- Add a line to an early `__pyc__/` file (e.g. a comment in
  `00_runtime.py`) and confirm no COMPILE-OUT check breaks.
