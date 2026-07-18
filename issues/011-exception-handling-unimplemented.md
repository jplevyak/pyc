# Issue 011: Exception handling (`try`/`except`/`finally`/`raise`) is unimplemented

**Status:** IMPLEMENTED 2026-07-17 (option C: exception slot +
explicit checks). `raise`/`try`/`except`/`else`/`finally`, typed
clauses (`except X as e:`, tuple forms), bare re-raise, and
cross-function propagation all work on both backends. Landing it
surfaced and fixed FOUR pre-existing, unrelated bugs:

1. **`_CG_prim_isinstance` was a stub** (`pyc_c_runtime.h`) — only
   handled the `isinstance(x, NoneType)` case; any check against a
   real class hard-coded `0` (always False). Never noticed before
   because nothing previously forced isinstance() against a
   non-constant-foldable value to reach codegen at all. Fixed:
   `cg.cc`/`cg_emit_llvm.cc` now emit a compile-time disjunction over
   the checked class's `implementors` set (the same one
   `ifa/analysis/fa.cc`'s own constant-folding uses) instead of
   calling through the macro for that case.
2. **`pass`-only subclasses got the wrong `__pyc_tag`** and silently
   dropped constructor arguments meant for an inherited `__init__`
   (`class Foo(Exception): pass` — the overwhelmingly common shape
   for user exception classes). Root cause: `gen_class_pyda`'s
   `__new__`-wrapper only looked up `__init__` in the class's own
   lexical scope, falling to a trivial "return self" synthesis
   whenever a subclass didn't define its own — losing the real
   parameter list even though the *dispatch* already resolved
   correctly through inheritance. Fixed in
   `python_ifa_build_syms.cc`'s `gen_class_pyda` (searches
   `cls->includes` before synthesizing).
3. **`build_isinstance_call`'s shared, clonable `isinstance()`
   wrapper** (`__pyc__/05_builtins.py`) gets generalized by FA into
   ONE polymorphic clone (runtime class arg) once the program checks
   more than one distinct class anywhere — silently breaking
   per-class dispatch. Fixed by emitting the raw `sym_primitive`
   isinstance send directly at each `except` clause's own call site
   instead (never shared).
4. A function whose entire body is an unconditional `raise` (no
   other `return`) left `fn->ret` with zero reaching definitions,
   surfacing as an untraceable "expression has no type" FA violation.
   Fixed with a syntactic pre-scan (`pyda_contains_return`, mirrors
   the existing `pyda_contains_yield` generator detection) so
   `goto_exc_target` knows — before the body is built — whether this
   is the function's only possible exit.

Verified: `tests/exception_basic.py`, `tests/exception_propagation.py`,
`tests/exception_assert.py` (deterministic, both backends); suites
203/0 × 2 backends; unit 58/0; IR 20/0. `__pyc_assert_fail__` now
raises a catchable `AssertionError(msg)` instead of print+exit
(`tests/assert_fail.py` updated to match).

**Per-callee can-raise gating (2026-07-18), closing the staged
plan's stage 2 properly**: the initial landing above gated the
post-call check on a single WHOLE-PROGRAM flag
(`pyc_program_has_raise`) — zero overhead only if the entire program
never raises anywhere, not per call subtree as the design intended.
Verified concretely: a `pure_math()` helper with no `raise` in its
own body or anywhere it calls STILL got a check inserted after every
call to it, solely because some unrelated function elsewhere in the
same program used `try`/`raise`. Fixed with `Sym::can_raise`
(`ifa/if1/sym.h`) — a whole-program fixed point
(`compute_can_raise`/`collect_can_raise`, `python_ifa_build_syms.cc`)
over a SYNTACTIC call graph, run once over the builtin module
(`ast_to_if1_baseline`, self-contained, shared via CoW across REPL
fork children) and once over user modules (`ast_to_if1_extend`) —
BEFORE any `build_if1` runs, so `emit_exc_check` can skip emitting
ANYTHING for a call whose resolved callee is proven `can_raise ==
false`. Conservative by construction: only a "plain call" (a bare
name in USE context immediately followed by a call trailer) resolves
to a specific callee at all; method dispatch, constructor calls, and
calls through a variable holding a callable always fall back to the
whole-program gate, unchanged from before. `tests/exception_propagation.py`
extended with `pure_math`/`mixed_caller` to lock in FUNCTIONAL
correctness of the resolution (not just the optimization) — a
resolution bug here would silently omit a NEEDED check, not just
miss zero-overhead; caught and fixed one during development
(`cur_val` at a plain call's build site can be a fresh, unnamed
load-temp for a top-level function reference — `PY_name`'s
`is_module_data_var` path — rather than the stable Sym
`collect_can_raise` resolved against; must resolve from `atom_ast->sym`
instead, and only when the call trailer sits directly on the atom).
Confirmed via `-x 1` IF1 dumps: a leaf helper's IF1 body is just the
call/move/reply with no slot read, isinstance check, or branch at
all — not merely folded away later.

Not done (out of scope for this pass, no corpus need yet): runtime
helpers (index/key errors) raising instead of trapping — issue 011's
staged-plan item 5; a memo/identity cache isn't needed since
exceptions aren't deep-copied.

Original report follows.

**Status (original):** open.
**Affects:** `python_ifa_build_if1.cc:1261-1269` (`build_if1_pyda`
— `PY_raise_stmt`, `PY_try_stmt`, `PY_except_clause`,
`PY_except_handler`, `PY_finally_clause` all hit the same
`fail(...)`).
**Related:** issue 012 (`with` statement — same `fail()` call
site, different feature); `issues/closed/005-while-true-fa-crash.md`
mentions a fib-heap idiom (`while True: ... break`) that had to
route around a *different* crash — exception handling was never
attempted in that work because it hits this wall immediately.
`closed/013-assert-statement-unimplemented.md` — `assert` landed a
minimal "abort the process" version explicitly *not* depending on
this issue; once this lands, `__pyc_assert_fail__`
(`__pyc__/05_builtins.py`) should be upgraded to raise a catchable
`AssertionError(msg)` instead of printing + `exit(1)`.

## Symptom

The grammar (`python.g:182-188` `try_stmt`, `:116` `raise_stmt`)
parses `try`/`except`/`finally`/`raise` without error, but the
frontend lowering rejects all of them identically:

```python
try:
    x = 1
except Exception:
    x = 2
print(x)
```

```
fail: error line 1, statement not supported in pyda path
```

Same message, same line, for a bare `raise ValueError("bad")`.

No test in `tests/*.py` exercises `try`/`except`/`raise`/`finally`
(confirmed via `grep` across the corpus) — this is an intentional,
total, cleanly-rejected gap, not a partially-working feature.

## Root cause

`build_if1_pyda` has no case for any of the exception-handling AST
kinds; they're grouped into one `fail()`:

```cpp
case PY_yield_stmt:
case PY_raise_stmt:
case PY_try_stmt:
case PY_except_clause:
case PY_except_handler:
case PY_finally_clause:
case PY_with_stmt:
case PY_with_item:
  fail("error line %d, statement not supported in pyda path", ctx.lineno);
  return -1;
```

There is no exception-object representation, no unwinding model,
and no IF1-level construct for "this send may transfer control to
a handler" in the current IR — this is a from-scratch subsystem,
not a small gap.

## Design alternatives analyzed (2026-07-15) — supersedes the sketch below

State that changed since filing: exception CLASSES now exist
(`__pyc__/08_exception.py`, the full standard hierarchy); `try`
no longer fails (body + else/finally build, except handlers
skipped); `raise` evaluates its expression and falls through. The
generated code is compiled as C++23 (`Makefile.cg`, clang++, NO
`-fno-exceptions`), already contains C++20 coroutines
(async/generators), and pyc's iterator protocol does NOT use
StopIteration (no hot-path exception tax to design around).

### A. Native C++ exceptions (what shedskin does)

`raise` → `throw (obj*)`; `try` → C++ `try/catch` + isinstance
dispatch in the handler.
- Happy path: zero cost (Itanium table-based unwinding). Throw:
  expensive (unwinder + RTTI), fine for exceptional paths.
- C backend: moderate work. GC'd, destructor-free generated code is
  actually favorable (no exception-safety/RAII concerns).
- **LLVM backend: a large project** — every potentially-throwing
  call becomes `invoke` with landingpads, personality function,
  typeinfo plumbing. Nothing exists for it today.
- Coroutines: propagation across resume boundaries routes through
  `promise_type::unhandled_exception()` — workable but subtle,
  interacts with the issue-014 generator machinery.
- FA: the throw MECHANISM doesn't remove any analysis work — FA
  still needs handler CFG edges and exception-value typing to
  compile `except X as e:` at all. Native EH just makes the FA
  precision unable to buy anything (tables are free regardless).

### B. setjmp/longjmp

- Per-`try` happy-path cost (register save + handler-stack
  push/pop).
- **Disqualifying hazard**: locals modified between `setjmp` and
  `longjmp` are indeterminate unless `volatile` — with `-O3` on
  generated code full of locals, that is a standing miscompile
  class (or a blanket-volatile optimization kill).
- `longjmp` across suspended C++20 coroutine frames is broken.
- Right answer for a pure-C89 backend circa 2005; wrong here.

### C. Exception slot + explicit checks, FA-gated (recommended)

`raise` stores the exception object into a per-thread slot and
branches (to the local handler if one encloses, else the
function-exit propagate path); after each call **that FA proves can
raise**, codegen emits `if (unlikely(_CG_exc_pending)) goto
Lhandler_or_propagate;`.

Why this fits pyc specifically:
- **IFA compatibility is maximal.** try/except lowers to ordinary
  IF1 labels/gotos — real CFG edges through the existing
  Code_LABEL/GOTO/IF vocabulary, no new IR concept — so SSU,
  dominators, liveness, and the (issue-033-fragile) splitter all
  work unchanged. The handler's exception var is typed by ordinary
  flow along those edges from the raise sites that reach it.
  `except X as e:` matching IS `isinstance` — the existing
  RP_IsInstanceOf restrict-predicate narrowing does the per-clause
  binding and dead-clause pruning with zero new machinery.
- **Whole-program can-raise gating** is where pyc's architecture
  pays uniquely: a monotone per-Fun (even per-EntrySet-clone) bit —
  same shape as the existing `fun_returns_value` — seeded by
  `raise` statements, propagated over call edges. Most clones in
  shedskin-style code provably cannot raise → NO check emitted →
  the happy path is literally untouched for them. Neither A nor B
  can convert that whole-program knowledge into anything.
- **Backend-uniform**: one compare+branch works identically in the
  C and LLVM emitters; nothing new in either. The slot survives
  coroutine suspension; generators check after resume.
- Raise cost: store + one branch per propagated frame — beats
  unwinding for the shallow propagation typical of Python
  error-handling; loses only for very deep propagation of frequent
  exceptions, which pyc's protocol design already avoids
  (no StopIteration iteration).
- Cost honestly stated: one predictable branch per can-raise call
  site plus the propagate-path code, only inside the raising
  subgraph.

### D. Handler-continuation passing (CPS)

Hidden handler-continuation argument on every call. Invasive across
IR/ABI/codegen; incompatible with the direct-call codegen shape.
Dismissed.

### Recommendation

**C, framed so the backend mechanism stays swappable**: land the
FA/IF1 layer (explicit handler edges, can-raise bit, slot
semantics) as THE design, with flag-checks as the emission strategy
in both backends. If profiling ever shows check overhead that
matters, the C backend can swap emission to native throw/catch
under the SAME analysis (A and C share the FA layer entirely; only
the emission differs) — that keeps today's cost low and the
analysis story singular.

Staged plan:
1. can-raise FA bit + `raise` lowering + intra-function
   `try/except:` (single bare handler) — replaces the current
   silent raise-fallthrough with real control flow.
2. Cross-function propagation (the check-after-call emission,
   gated on callee can-raise).
3. Typed clauses + `except X as e:` binding via RP_IsInstanceOf
   narrowing; clause order = first-match dispatch.
4. `finally` (code duplication first — matches the existing
   lowering style), bare re-raise from the slot.
5. Follow-ons: upgrade `__pyc_assert_fail__` to raise
   AssertionError; make runtime helpers (index/key errors) raise
   instead of trap ONLY where FA sees an enclosing handler.

---

## Original fix sketch (superseded by the analysis above)

This is the largest of the "missing core feature" issues filed in
this batch; a real implementation needs decisions at multiple
layers:

1. **IF1/runtime representation**: pyc has no exception object
   type today. Needs a base `Exception`-like class in `__pyc__/`
   plus a runtime unwind mechanism. Two common compiler strategies:
   - **setjmp/longjmp-style** in the C backend (simplest to bolt
     onto the existing direct-C codegen; matches the "value type /
     no GC pause" style of the rest of the runtime), or
   - **status-code threading** (each call site checks a
     "did this raise" flag/return-channel and early-returns up the
     call chain) — more explicit in the generated code, easier to
     reason about for FA, but touches every call site's codegen.
2. **Frontend lowering**: `try`/`except`/`finally` need to become
   IF1 constructs FA can reason about — at minimum, the `except`
   body must be reachable from any send inside the `try` body (a
   different control-flow shape than the current straight-line
   Code_SEQ/Code_IF/Code_GOTO model), which likely needs new
   groundwork in `ifa/optimize/cfg.cc`/`ifa/if1/pnode.h` before
   FA/codegen can process it at all.
3. **`raise`** needs to at least support re-raising the currently
   propagating exception (bare `raise` inside an `except` block)
   and constructing+raising a new exception instance.
4. Given the scope, consider landing a **minimal first slice**:
   `try`/`except Exception:`/`finally` with a single unconditional
   handler (no exception-type matching, no `except as e:` binding)
   before tackling typed multi-clause `except` matching.

## Verification plan

1. Minimal: `try: risky() except: handle()` runs the handler when
   `risky()` (implemented as `raise ValueError()` or similar)
   raises, and skips it otherwise.
2. `finally` runs on both the exception and no-exception paths.
3. `except Exception as e:` binds the exception object.
4. Re-raise (`raise` with no argument inside an `except` block).
5. Add `tests/exception_basic.py`, `tests/exception_finally.py`,
   `tests/exception_reraise.py` + `.exec.check` files — currently
   zero coverage.

## What this unblocks

Exception handling is fundamental to idiomatic Python — file I/O,
dict/list bounds checking, parsing, and virtually all
error-handling code relies on it. It's likely the single largest
category of "why won't my ported script compile" for real-world
Python code.
