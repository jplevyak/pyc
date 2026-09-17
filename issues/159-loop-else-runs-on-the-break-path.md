# issues/159 — a loop's `else` ran on the `break` path

**Status:** fixed 2026-09-17.

## The bug

Python's loop `else` runs **only if the loop finished without `break`**. pyc
emitted it unconditionally after the loop.

`if1_loop` (`ifa/if1/if1.cc:350`) uses ONE label for both the normal exit and
the break target:

```c
if1_if_label_false(p, if_goto, brk);   // loop condition false -> brk
if1_label(p, t, ast, brk);             // and the break label is placed here
```

so the caller's `if1_gen(if1, &ast->code, orelse)` landed after both paths.

## Why it was invisible

It surfaced as `expression has no type`, not as a wrong answer, because the
`else` in the idiom that exposes it *raises*:

```python
for edge in [...]:
    d = edge.d
    break
else:
    raise AssertionError
return d            # <- pyc: unreachable, so `d` and everything after is NOTYPE
```

The unconditional raise made the post-loop code unreachable, so the analysis
reported NOTYPE there. Under the old severity those were **warnings**, the
program compiled, and the binary was wrong:

```
pyc binary: Unhandled exception:
CPython:    1.0
```

**A miscompile shipping as three warnings.** It was found within minutes of
[ifa/158](../ifa/issues/158-FA-every-type-violation-is-fatal.md) making
violations fatal, which is the case for that change.

## The fix

Give the loop its own exit label, emit `orelse` there, and place the BREAK
label after it, so `break` jumps past the else. Both `PY_for_stmt` and
`PY_while_stmt`; only when an `else` clause exists, so loops without one are
byte-identical.

## Verification

| | CPython | pyc before | pyc after |
| --- | --- | --- | --- |
| `for`/`break`/`else: raise` | `1.0` | `Unhandled exception` | **`1.0`** |
| `for`/`break`, no else | `1.0` | `1.0` | `1.0` |
| `for`/`else: raise`, no break | `AssertionError` | 3 warnings | **refuses** (correct — the code really is unreachable) |
| `for`/`else: pass` | `2.0` | `2.0` | `2.0` |

`shedskin_examples/dijkstra`: **6 errors → 0 errors, 0 warnings**, compiles and
runs. All six were this one bug, reported at the definition of `distance` and
at both call sites.
