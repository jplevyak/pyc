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

## Two sibling divergences, found the same way — both FIXED 2026-08-28

Delta-reducing `shedskin_examples/linalg` for
[ifa/105](../ifa/issues/105-type-degeneration-in-shared-generic-methods.md)
surfaced two more places where pyc's front end and CPython disagree.
Both are fixed; recorded here because this issue is where the "pyc's
parser is not a proxy for Python's" rule lives, and because the second
one silently invalidated a whole reduction run before it was noticed.

**1. A file with no trailing newline was a syntax error.** CPython's
tokenizer synthesizes a NEWLINE at end of input, so `print(1)` with no
final `\n` is legal Python. `python.g`'s `file_input: (NL | stmt)*` has
no such rule, so DParser reported `syntax error after ')'` on the FINAL
statement. Fixed in `python_parse.cc` with `ensure_trailing_newline()`,
copying what `dparse_builtin_dir` already does when it concatenates the
builtin files.

This is exactly the trap this issue warns about, from the other
direction: `ast.unparse` emits no trailing newline, so every candidate
an AST-based reducer writes was rejected by the parser rather than by
the oracle.

**2. A parse error exited 0.** `pyc.cc`'s module loop simply did not add
an unparseable file to `mods`; with only the builtin left, the
`mods.n > 1` guard skipped compilation and control fell to `exit(0)`.
pyc printed `dparse: parse error in 'f.py' near line 2`, produced no
binary, and reported **success** — so anything reading the exit code saw
a clean build. The reducer read it as "this candidate compiles fine",
which is how the first divergence stayed invisible. Fixed by counting
parse failures and exiting 1.

Neither changes the empty-`if:` body above, which is still open.
