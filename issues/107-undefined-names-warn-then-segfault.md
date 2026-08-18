# 107 — an undefined name compiles (exit 0) and the binary segfaults

**Status:** open, found 2026-08-18 while delta-reducing `plcfrs` for
[ifa/issues/105](../ifa/issues/105-type-degeneration-in-shared-generic-methods.md).
Repro: `tests/undefined_name_executed.py` (`.known_issue`).

**Target: make undefined-name handling consistent with CPython** — a
clean, named error, never a warning-then-crash.

## Symptom

```python
print(NoSuchName)
```

| | result |
|---|---|
| CPython | `NameError: name 'NoSuchName' is not defined` |
| **pyc** | three warnings, **exit code 0**, and a binary that **segfaults** |

pyc's diagnostics for that one line:

```
warning: 'NoSuchName' has no type
u2.py:1: warning: illegal call argument type expression illegal:
    print(NoSuchName)
    ^
u2.py:1:6: warning: expression has no type
```

None of them names the actual problem ("undefined name"), none is an
error, the compile *succeeds*, and the program then dies.

## Full behaviour, measured

| case | CPython | pyc |
|---|---|---|
| undefined name in an **unexecuted** function | `ok` (never evaluated) | **no diagnostic at all**, runs `ok` |
| undefined class in an **unexecuted** method | `ok` | **no diagnostic at all**, runs `ok` |
| undefined name at module level (**executed**) | `NameError` | warnings, exit 0, **SIGSEGV** |
| undefined name in a called function (**executed**) | `NameError` | warnings, exit 0, **SIGSEGV** |

So pyc matches CPython on the unexecuted cases only by accident — it says
nothing in *any* case, and where CPython reports a clean error pyc
produces a crashing binary.

## Why it matters beyond the crash

1. **A segfault is the worst possible reporting** of a condition the
   compiler has already detected — it emitted a warning about it.
2. **It defeats static reduction.** This was found because a delta
   reducer deleted every class definition, left the call sites, and pyc
   kept accepting the result — so four successive reduction oracles
   produced "reproducers" that were really pyc inferring over garbage.
   See [105](../ifa/issues/105-type-degeneration-in-shared-generic-methods.md)'s
   oracle table; `ifa/issues/repro/nameck.py` exists solely to work
   around this.
3. **It can fabricate type degeneration.** An undefined name has *no*
   type, and the resulting bottom/unknown propagates — which is exactly
   the phenomenon 105 is investigating. Any analysis of a program
   containing undefined names is untrustworthy.

## Target behaviour

Consistent with CPython, and taking advantage of pyc being static:

- **A reached undefined name is a hard compile error**, not a warning:
  `error: name 'NoSuchName' is not defined`, at the use site, with a
  non-zero exit. pyc knows this at compile time and can do strictly
  better than CPython's runtime `NameError`.
- **An unreached undefined name is at least a warning** naming the
  symbol. It is legal Python (names resolve at runtime), so an error may
  be too strong by default — but silence is wrong, and a flag to make it
  an error is worth having, since it is almost always a bug.
- **Never a segfault.** If codegen cannot emit a call whose target is
  unknown, that is the existing
  [102](../ifa/issues/102-corpus-programs-compile-then-abort-at-runtime.md)
  problem (`cg.cc:2055` emits an abort stub rather than failing the
  build); an undefined name should not reach codegen at all.
- **Say what is wrong.** `'X' has no type` and `expression has no type`
  describe the analyser's internal state, not the user's mistake. The
  wording should name the undefined symbol, in CPython's terms.

This is the same lesson recorded against shedskin's diagnostics in
[018](018-dict-mixed-key-types-boxing-failure.md): shedskin reports
`*WARNING* Variable 'x' has dynamic (sub)type: {float, list}` where pyc
reports an internal `sizeof_element` assertion. Naming the user-level
problem is the cheap, high-value half of these issues.

## Verification plan

- `tests/undefined_name_executed.py` reports an error and exits non-zero;
  delete its `.known_issue` tag.
- The unexecuted-name cases produce a warning naming the symbol.
- No corpus program regresses: none should contain undefined names, but
  `ifa/issues/repro/nameck.py` can confirm that in one pass, and if any
  does, that is itself a finding.
- `ifa/issues/repro/check5.sh` can then drop its `nameck.py` step.
