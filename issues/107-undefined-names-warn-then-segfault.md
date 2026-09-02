# 107 — an undefined name compiles (exit 0) and the binary segfaults

**Status: open — mostly fixed 2026-08-18 (`c8d7da8d`), ONE shape
remains.** An undefined name is now a compile error naming the symbol,
with a non-zero exit — see "The fix" at the end — **except when the name
appears as a call argument**, which is exactly the shape this issue's
own test uses, and why `tests/undefined_name_executed.py` still carries
a `.known_issue` tag.

Measured 2026-08-22, on `c8d7da8d`'s own code (not a regression — this
was never covered):

```
x = NoSuchName            -> error line 1, name 'NoSuchName' is not defined   rc=1
def f(): return NoSuchName -> error line 2, ...                              rc=1
print(NoSuchName)         -> three "has no type" warnings                     rc=0
f(NoSuchName)             -> same, for a user-defined f                       rc=0
len(NoSuchName)           -> same, for a builtin                              rc=0
```

So `report_undefined_names` never sees the use: the argument position
does not record a pending use the way an ordinary load does. The status
line previously read a flat "FIXED", which is how the gap survived — the
`.known_issue` tag naming this issue was the only thing still telling
the truth.

**Originally filed** 2026-08-18 while delta-reducing `plcfrs` for
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
[018](closed/018-dict-mixed-key-types-boxing-failure.md): shedskin reports
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


## The fix

`make_PycSymbol`'s `PYC_USE` case did `if (!l) goto Lglobal;` — an
unresolved name **minted a fresh module global**, which then had no type,
warned, compiled with exit 0, and segfaulted.

It now records the unresolved use and defers the verdict to
`report_undefined_names()` at end of module, because **forward references
legitimately take the same path** (`def f(): return g()` before
`def g()`). Anything still unbound once the module is walked is reported:

```
error line 11, name 'NoSuchName' is not defined
fail: 1 undefined name
```

with exit code **1**.

### Four things this surfaced, each fixed

1. **The compiler segfaulted while reporting the error** (rc=139) —
   returning a code let the caller unwind into a half-built IF1. It now
   exits through `fail()` like every other frontend error. That is the
   very failure mode this issue exists to remove, reproduced inside the
   fix itself.
2. **Scope-insensitive binding tracking.** The first version marked a
   name "bound" from *any* scope, so a local of one function resolved an
   unresolved global read in another. Only module-scope bindings can now
   resolve a module-scope fallthrough — the identical bug I had just
   fixed in the reducer's own `nameck.py`.
3. **Keyword-argument names are not variable reads.** `print(x, end=" ")`
   was reporting `end`. Suppressed via `ctx.in_kwarg_key`; note that
   *skipping* the key child instead broke 283 tests, so it must still be
   walked.
4. **`with … as X` never bound `X`** —
   [108](closed/108-async-with-as-target-not-bound.md), which looked
   async-specific only because of defect 2. `PY_with_item` now marks its
   target `PY_STORE`. **Fixed and closed.**

`_` (the `match` wildcard) is never reported.

### Corpus effect: three runtime crashes became compile errors

| program | before | after |
|---|---|---|
| `rdb` | compiled, **exited 1 at runtime** | `error: name 'EOFError' is not defined` |
| `sunfish` | compiled, **SIGABRT** | `error: builtin 'divmod' is not supported by pyc` |
| `voronoi2` | compiled, **SIGABRT** | `error: builtin 'property' is not supported by pyc` |

All three referenced CPython builtins pyc does not implement, and all
three were already in
[102](../ifa/issues/102-corpus-programs-compile-then-abort-at-runtime.md)'s
crash list. Turning those into compile-time diagnostics is exactly 102's
stated goal, so the three exit-code changes are an improvement, not a
regression. Unimplemented builtins get their own wording — the user did
not misspell anything.

Suite: 274 passed / 14 known / 0 failed. The other 74 corpus programs are
unchanged.
