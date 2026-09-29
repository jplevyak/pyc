# 106 — pyc accepts an `if:` with no body inside a function

**Status:** CLOSED — fixed 2026-09-25. Found 2026-08-18 while delta-reducing `plcfrs` for
[ifa/issues/105](../../ifa/issues/closed/105-type-degeneration-in-shared-generic-methods.md).
Repro: `tests/empty_if_body_accepted.py` (now `.check_fail`).

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

## Resolution (2026-09-25)

**Root cause.** `INDENT` (`python_indent` in `python.g`) only asks whether
the indent stack's top exceeds the entry below it. It never compares the
body with the header. Inside a function the stack still holds the function
body's own push (`[0, 4]`), so an `if` at column 4 followed by a line at
column 4 passes `4 > 0`, and that line is parsed as the `if`'s body. At
module level the stack is `[0]` and the test fails, which is why only the
nested shape was accepted.

**Fix.** `py_suite_deeper` requires an indented suite's first statement to
be indented deeper than the line its header starts on (`py_header_indent`,
which looks through `async`). It runs as a speculative action on every rule
that takes a `suite` — `def`, `class`, `if`/`elif`/`else`, `while`, `for`,
`try`/`except`/`finally`, `with` and `case` — and on `match` for its first
`case`. A one-line suite (`if x: pass`) has no indent and is not checked.

**Verified.**
- Thirteen malformed shapes inside an `async def` (each compound form with
  a same-column body, plus `match`/`case`, `finally` and `async for`) are
  all rejected, as CPython rejects them. Before the fix, `if`, `match` and
  nested `def` compiled with rc=0.
- A program using every compound form nested in a method — including a
  continuation-line `if` header, one-line suites and nested `async def` —
  prints CPython's output.
- `--dparse-only` over all 614 tracked `.py` files agrees with CPython's
  `ast.parse` on 613. The exception, `test_pyc.py`, fails identically on
  the pre-fix binary. So nothing in the corpus or `__pyc__/` relied on the
  leniency.

**Left as is: the location.** The error is reported at end of file, not at
the `if` (CPython: the line after it). The check is a speculative action
on the compound statement, and DParser runs it only when it reduces that
statement, at the final DEDENT. Pointing at the right line needs `INDENT`
itself to know the header's indent — a change to the indent-stack
tokenizer, which the whole corpus depends on.

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

## Verification plan (original)

- `tests/empty_if_body_accepted.py` reports an error instead of printing
  `2`; delete its `.known_issue` tag.
- The module-level case keeps its existing `dparse: parse error`.
- Sweep the corpus for programs that currently rely on this leniency —
  there should be none, but a silent branch drop would be invisible
  otherwise.

## Two sibling divergences, found the same way — both FIXED 2026-08-28

Delta-reducing `shedskin_examples/linalg` for
[ifa/105](../../ifa/issues/closed/105-type-degeneration-in-shared-generic-methods.md)
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
