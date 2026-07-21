# Issue 012: `with` statement (context managers) is unimplemented

**Status:** CLOSED 2026-07-04 (`4401b1ad`, cleanups in `c14a214b`,
moved to `closed/` in `22fb6f3c`). The happy-path desugaring this
issue's own fix sketch proposed landed and works: `with EXPR as
VAR:`/`with EXPR:`, multiple context managers in one statement
(`with a as x, b as y:`), and `__exit__` cleanup firing correctly on
normal fallthrough, `return`, and `break`/`continue` out of an
enclosing loop (tracked via a `ctx.with_stack` the frontend threads
through those exit points). Re-verified 2026-07-20 by actually
compiling and running `tests/with_basic.py`, `tests/with_break.py`,
and `tests/with_return.py` and diffing byte-for-byte against real
`python3` output — all three match exactly. (This file had been
moved to `closed/` with no content update — the "Status: open" line
above and the rest of this file were stale until this pass; the
2026-07-04 commits above are the actual close.)

**Known gap, now concretely confirmed and filed separately:**
`__exit__` is never called when the `with` body raises, and can't
suppress the exception the way real Python allows (`__exit__`
returning truthy). This is exactly the gap this issue's own original
text flagged as depending on exception-unwinding support — issue 011
landed 2026-07-17, well after this issue was closed, so the
dependency was never revisited. Confirmed via direct test: pyc's
`with` skips `__exit__` and the "after with" line entirely when the
body raises, diverging from CPython. Filed as
[030](../030-with-exit-not-called-on-exception.md).

**Affects:** `python_ifa_build_if1.cc` (`build_if1_with_items`,
`build_if1_assign_target` — new; `PY_with_stmt` case; `ctx.with_stack`
cleanup emission at `PY_return_stmt`/`PY_break_stmt`/`PY_continue_stmt`);
`python_ifa_int.h` (`WithCleanup`, `PycCompiler::with_stack`,
`loop_depth`).
**Related:** [011](011-exception-handling-unimplemented.md)
(exception handling — landed 2026-07-17, three weeks after this
issue closed; its unwinding path was never wired into `with`'s
cleanup, hence issue 030); [030](../030-with-exit-not-called-on-exception.md)
(the exception-safety follow-on this issue's own text anticipated).

## Symptom

The grammar (`python.g:189` `with_stmt`) parses `with` without
error; the frontend rejects it:

```python
with open("x") as f:
    pass
```

```
fail: error line 1, statement not supported in pyda path
```

No test in `tests/*.py` exercises `with` (confirmed via `grep`
across the corpus) — total, cleanly-rejected gap.

## Root cause

Same shared `fail()` call as issue 011:

```cpp
case PY_with_stmt:
case PY_with_item:
  fail("error line %d, statement not supported in pyda path", ctx.lineno);
  return -1;
```

No lowering exists for the context-manager protocol
(`__enter__`/`__exit__`).

## Proposed fix sketch

Unlike `try`/`except`, a **basic** version of `with` doesn't need
full exception-unwinding to be useful:

```python
with EXPR as VAR:
    BODY
```

desugars (ignoring exception propagation) to:

```python
VAR = EXPR.__enter__()
BODY
VAR.__exit__(None, None, None)
```

This slice is implementable purely as a frontend AST-to-AST (or
direct-to-IF1) rewrite in `build_if1_pyda`'s `PY_with_stmt` case,
reusing the existing `call_method` helper the same way
`PY_dict`'s `__setitem__` calls do — **no new IF1 constructs
needed** for this minimal form, unlike issue 011.

The gap versus real Python semantics: `__exit__` must run even if
`BODY` raises (and receives the exception info, and can suppress
it by returning truthy). That part **does** depend on issue 011's
unwinding mechanism landing first. Land the happy-path desugaring
now; note the exception-safety gap in a follow-up once issue 011
exists.

## What landed

`build_if1_with_items` (`python_ifa_build_if1.cc`) recursively lowers
each `with_item` in one statement: evaluates the context-manager
expression, calls `__enter__`, binds the optional `as` target
(reusing a new shared `build_if1_assign_target` helper — factored out
of `PY_assign`, also used for destructuring targets), recurses into
the remaining items/body, then emits `__exit__(None, None, None)`
after. A `ctx.with_stack` (added in the `c14a214b` cleanup pass)
records each active context manager's value alongside the current
`loop_depth`; `PY_return_stmt` unwinds and calls `__exit__` on every
entry before jumping to the function's exit label, and
`PY_break_stmt`/`PY_continue_stmt` do the same for every entry opened
at or inside the current loop depth (`loop_depth` incremented/
decremented around `for`/`while` bodies for exactly this check).

## Verification plan

1. `with EXPR as VAR: BODY` runs `__enter__`, binds `VAR`, runs
   `BODY`, then runs `__exit__`. ✓ (`tests/with_basic.py`, "test 1")
2. `with EXPR:` (no `as` clause) still calls `__enter__`/`__exit__`
   without binding a name. ✓ (`tests/with_basic.py`, "test 3")
3. Multiple context managers in one `with` (`with a, b:` /
   `with a as x, b as y:`). ✓ (`tests/with_basic.py`, "test 2")
4. (Follow-on, blocked on issue 011 at filing time) `__exit__` still
   runs when `BODY` raises. **Still broken** — issue 011 has since
   landed but this was never revisited; confirmed by direct test and
   filed as [030](../030-with-exit-not-called-on-exception.md).
5. Add `tests/with_basic.py` + `.exec.check` — done; also
   `tests/with_break.py` (break out of a loop with nested `with`s)
   and `tests/with_return.py` (return from inside a `with`), both
   also missing `.exec.check` until this pass (they existed and
   compiled but ran **compile-only** under `test_pyc.py` — no golden
   output to diff against, so their cleanup-ordering behavior was
   never actually execution-verified until now). All three now added
   and pass `test_pyc.py`, execution-verified against real `python3`
   output byte-for-byte on the C backend.

## What this unblocks

`with` is the standard idiom for resource management (files,
locks, connections) in Python; a happy-path implementation covers
the overwhelming majority of real-world usage even before
exception-safety lands (tracked separately now as issue 030).
