# 033 — `and`/`or` inside a comprehension `if`-filter builds the crash-prone value-preserving union instead of the boolean-context bool-only form

**Status: FIXED 2026-08-06.** Found investigating
`shedskin_examples/yopyra/yopyra.py`, which failed to compile at all
(`fail: mismatched field sizes: class 'closure' field '<anon>' mixes
8- and 1-byte members ('bool')`).
**Affects:** `python_ifa_build_if1.cc`'s `PY_bool_and`/`PY_bool_or`
case, specifically the `in_boolean_context` lambda (~2975-2985).
**Related:** [ifa/issues/closed/071](../../ifa/issues/closed/071-chess-accumulated-union-notype-cascade.md)
— the *exact same* crash signature (`mismatched field sizes: class
'closure'...`), a different union (`bool | None` there vs. `bool |
str` here) reaching the same `clone.cc:determine_layouts` failure via
the same mechanism (an unboxed union landing in a bound-method
partial-application closure field). 071's fix targeted two *other*
sources of that union (implicit fall-off-`None`, a lambda
`fun_returns_value` gap); this issue is a third, independent source
this comment block's own code was already explicitly designed to
prevent — just not for this one AST position.

## Symptom

Minimal repro (reduced from `yopyra.py:183`'s
`[l.split() for l in open(scene_filename) if l.strip() and
l.strip()[0] != "#"]`):

```python
lines = [l.split() for l in ["a b", "", "# comment", "c d"]
         if l.strip() and l.strip()[0] != "#"]
print(lines)
```

```
warning: expression has mixed basic types:( bool str )
mismatched field members: str(8) bool(1)
fail: mismatched field sizes: class 'closure' field '<anon>' mixes 8- and 1-byte members ('bool')
```

A structurally identical plain `if`/`while` (no comprehension) with
the same `and` expression compiles and runs fine — the gap is
specific to comprehension filter clauses.

## Root cause

Python's `and`/`or` return one of their *operands* (short-circuit
value semantics), not necessarily a `bool` — `l.strip() and
l.strip()[0] != "#"` can yield either a `str` (the falsy `l.strip()`
itself) or a `bool` (the second operand). `PY_bool_and`/`PY_bool_or`'s
codegen already has a "BOOLEAN CONTEXT" optimization for exactly this
(added for issue 025, motivated by genetic2's `if node.args and
<test>:`): when the `and`/`or` result feeds directly into an `if`/
`while`/`elif` condition (or `not`, or a further `and`/`or` that
itself feeds one), only its *truthiness* is ever observed, so the
lowering can produce the per-operand `__pyc_to_bool__` result
directly instead of the full value-preserving union — the comment
there explicitly names this same "mismatched field sizes" crash class
as the reason. `in_boolean_context`'s parent-walk recognized
`PY_if_stmt`/`PY_while_stmt`/`PY_elif_clause` as boolean-context
parents but not `PY_list_if`/`PY_comp_if` — a comprehension's `if
<test>` filter clause, which `build_list_comp_inner_pyda`'s own
comment describes with the identical shape ("children = [test,
list_iter?]"), and is exactly as boolean-context as the others: the
filter test's operand value is never observed either, only whether it
passes the filter. Missing that one case meant every `and`/`or`
comprehension filter still built the crash-prone union.

## Fix

Added `PY_list_if`/`PY_comp_if` to the same condition
`in_boolean_context` already checks for the other three statement
kinds (same `p->children[0] == nn` shape holds for both).

## Verification

- Minimal repro above: compiles clean, `[['a', 'b'], ['c', 'd']]`,
  matches `python3` exactly.
- `yopyra.py`: no longer hits the fatal build error (exit 0, was exit
  1) — a real binary is produced. (A *second*, unrelated gap remains
  in `yopyra.py` specifically — `color`/`punto3d`'s `c += x` fails
  with `unresolved call '__iadd__'`, since those classes define
  `__add__` but not `__iadd__` and pyc's augmented-assignment lowering
  has no fallback to the non-in-place operator the way CPython's data
  model does. Not this issue's mechanism — tracked separately.)
- `ifa --test`: 58/58.
- `test_pyc.py`, C and LLVM backends, `PYC_CSM` unset: 240/11/0/4
  both (239 pre-issue-032 baseline + issue 032's 1 new test + 0 here,
  0 regressions).
- `test_pyc.py`, C and LLVM backends, `PYC_CSM=2`: 236/11/4/4 both,
  same 4 pre-existing failures.
- `shedskin_sweep.sh`, both `PYC_CSM` settings: one clean gain each
  (`yopyra`: `FAIL` → `COMPILED_C_WARN`), zero regressions, diffed
  directly against saved pre-fix `results.tsv`.

## What this unblocks

`and`/`or` used as a comprehension filter — an ordinary, common
Python idiom (`[x for x in xs if x and cond(x)]`) — no longer risks a
hard compiler crash whenever the operands' types happen to have
different unboxed C representation sizes. Not `yopyra.py`-specific:
any corpus or user program with this shape was equally broken before
this fix.
