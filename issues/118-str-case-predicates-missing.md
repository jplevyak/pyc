# 118 — str.islower / isspace / swapcase did not exist

**Status:** FIXED 2026-08-21, found while clearing issues/115's residue.

## Symptom

```python
print("z".isspace())
```

    CPython:  False
    pyc:      warning: illegal call argument type expression illegal:
              assert(!"runtime error: getter not resolved")

`__pyc__/01_str.py` defined `upper`, `lower` and `isupper` but not
`islower`, `isspace` or `swapcase`.

## Why it was hard to see

The diagnostic never says the method is missing. An unresolved method
yields a bottom-typed value, and what surfaces is whatever downstream
use first chokes on it — "illegal call argument type expression illegal:"
with an empty type, pointing at the enclosing statement. Reduced from
sunfish, the failure first looked like `break` inside a generator loop:
the only difference between a passing and a failing case was whether the
break condition happened to call a method that existed.

Worth filing separately (not done): **an unresolved method call should
name the method**. `unresolved call '__lt__'` exists for operators; a
plain `obj.foo()` with no matching `foo` gets nothing comparable.

## Fix

Added all three to `__pyc__/01_str.py`, ASCII-only and consistent with
the existing `upper`/`lower`/`isupper`:

- `islower` — the exact mirror of `isupper` (true iff some cased
  character exists and none is uppercase).
- `isspace` — true iff non-empty and every character is ASCII
  whitespace (space, `\t`, `\n`, `\v`, `\f`, `\r`).
- `swapcase` — per-character inversion.

## What this blocked

`shedskin_examples/sunfish`'s `gen_moves` uses `isspace` and `islower`
to find board edges and captures, and `rotate`/`nullmove` use
`swapcase`. With this and issues/114-117, `gen_moves` compiles clean;
sunfish's remaining diagnostics are elsewhere (`str.split()` with no
`sep`, and something reached through `re.py`).

Note the harness quirk this trips (issues/111): adding ~40 lines to
`__pyc__/01_str.py` shifted every later line number in the concatenated
`__pyc__.py`, so `tests/minmax_3arg.py.check` — which embeds them —
needed updating (1632 -> 1674, 1663 -> 1705). Only the numbers changed.

## Verification

`tests/str_case_predicates.py`, against CPython for a spread of inputs
including empty, all-whitespace, mixed-case and non-cased strings.
