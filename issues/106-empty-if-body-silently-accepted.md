# 106 — pyc accepts an `if:` with no body inside a function

**Status:** open, found 2026-08-18 while delta-reducing `plcfrs` for
[ifa/issues/105](../ifa/issues/105-type-degeneration-in-shared-generic-methods.md).
Repro: `tests/empty_if_body_accepted.py` (`.known_issue`).

## Symptom

```python
def f(a):
    if a == 1:
    b = 2
    return b
print(f(1))
```

| | result |
|---|---|
| CPython | `IndentationError: expected an indented block after 'if' statement on line 2` |
| **pyc** | compiles and prints **`2`** — the `if` is silently dropped |

At **module** level the same shape *is* rejected (`dparse: parse error in
… near line 3`), so this is specific to a suite inside an indented block.

## Still open after the 107 fix (2026-08-18)

[107](107-undefined-names-warn-then-segfault.md) removed the *other*
reason a reduction oracle needed `ast.parse` (undefined names), but this
one stands: pyc still accepts an empty `if:` body inside a function, so
`ast.parse` validation remains necessary for any Python reduction here.

## Why it matters beyond the parse

It silently discards a conditional. A program with this typo compiles and
runs, taking a branch unconditionally, with no diagnostic at all.

It also invalidates naive delta reduction of Python for pyc, which is how
it was found: 105's reduction ran `plcfrs` from 638 lines down to 93
while preserving the target error, but the 93-line result is **not valid
Python** — the oracle only checked for the error string, and pyc's parser
accepted every malformed intermediate. Any reducer must validate
candidates with `ast.parse` before consulting the oracle.

## Where to look

`python.g` / `python_parse.cc` — the grammar rule for a suite. The
module-level path already errors, so the indented-suite path is missing
the same check (or the parser is treating the dedented statement as
closing an empty suite).

## Verification plan

- `tests/empty_if_body_accepted.py` reports an error instead of printing
  `2`; delete its `.known_issue` tag.
- The module-level case keeps its existing `dparse: parse error`.
- Sweep the corpus for programs that currently rely on this leniency —
  there should be none, but a silent branch drop would be invisible
  otherwise.
