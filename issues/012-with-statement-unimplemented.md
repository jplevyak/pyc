# Issue 012: `with` statement (context managers) is unimplemented

**Status:** open.
**Affects:** `python_ifa_build_if1.cc:1261-1269` (`build_if1_pyda`
— `PY_with_stmt`, `PY_with_item` share the same `fail()` as issue
011's exception-handling kinds).
**Related:** issue 011 (exception handling — `with` doesn't
strictly require exception support if implemented as unconditional
`__enter__`/`__exit__` calls around the body, but real `with`
semantics call `__exit__` even when the body raises, which does
depend on unwinding).

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

## Verification plan

1. `with EXPR as VAR: BODY` runs `__enter__`, binds `VAR`, runs
   `BODY`, then runs `__exit__` — verify via a test class that
   appends to a log list in each method and asserting the
   expected order.
2. `with EXPR:` (no `as` clause) still calls `__enter__`/`__exit__`
   without binding a name.
3. Multiple context managers in one `with` (`with a, b:` /
   `with a as x, b as y:`).
4. (Follow-on, blocked on issue 011) `__exit__` still runs when
   `BODY` raises.
5. Add `tests/with_basic.py` + `.exec.check` — zero coverage
   today.

## What this unblocks

`with` is the standard idiom for resource management (files,
locks, connections) in Python; a happy-path implementation covers
the overwhelming majority of real-world usage even before
exception-safety lands.
