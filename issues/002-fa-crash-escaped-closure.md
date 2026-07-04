# Issue 002: Segfault when a bound-method closure escapes its binding scope

**Status:** fixed (2026-07-04). Case A (closure passed as a
function argument) was fixed earlier — see "2026-07
re-investigation" below. Case B (closure stored in and read back
from a **global** variable) is now also fixed: root cause was the
`None` initializer polluting the global's single flow-insensitive
GLOBAL_CONTOUR cell, typing it `SUM{__pyc_None_type__, closure}`,
which codegen's closure-unpacking path (`is_closure_var`, requiring
a bare `Type_FUN`) didn't recognize — see "2026-07-04 Case B root
cause and fix" at the bottom. Regression test:
`tests/closure_in_global.py` (both backends).
**Affects:** pyc frontend (suspected: `python_ifa_build_if1.cc`
or `analysis/fa.cc`); manifests when a closure created by `a.m`
crosses a function-call boundary — passed as an argument,
returned, or assigned to a global from inside a function.
**Related:** [001-fa-crash-captured-locals.md](001-fa-crash-captured-locals.md)
(both are gaps in pyc's closure model); v2 LLVM closure handler
work (commit `db4270a`). Issue 001's investigation (dug into `ifa`'s
`nesting_depth`/`display` machinery in depth) confirmed *that*
issue's root cause is specific to lambdas/nested-defs capturing
enclosing locals via `ifa`'s stack-disciplined nested-function
support — a mechanism bound methods (this issue's repro) don't use
at all (`self` is already threaded explicitly). This issue's crash
(a silent segfault, not `unique_AVar`'s assertion) is more likely a
separate bug in how a bound method's closure-struct type propagates
across a call-edge boundary (`lower_send_period`, see below) — worth
re-testing once issue 001's fix lands, but don't assume it's fixed
without checking.

## Symptom

Both of these crash pyc with a segfault (no assertion message —
the FA dereferences a null pointer somewhere downstream of the
closure shape mismatch):

```python
# Case A — closure passed as a function argument
class Counter:
  v = 0
  get = lambda y: y.v

def call_with(f):
  print(f())

c = Counter()
c.v = 42
call_with(c.get)
```

```python
# Case B — closure assigned to a global from inside a function
class Counter:
  v = 0
  get = lambda y: y.v

stash = None

def setup():
  global stash
  c = Counter()
  c.v = 42
  stash = c.get

def use():
  print(stash())

setup(); use()
```

Both crash silently (segfault, exit 139) — no error message,
no source location.  Both backends affected; the crash is in
the frontend before code generation runs.

## Contrast with what works

`tests/lambda_closure.py` exercises the same bound-method
pattern but keeps everything in `__main__`:

```python
class A:
  i = 3
  x = lambda y: y.i
a = A()
z = a.x        # closure created here
a.i = 4
print(z())     # called here, same scope
```

`tests/closure_in_loop.py` (added in commit `b24bfbb`) extends
this to a loop:

```python
f = a.get
while i < 5:
  total = total + f()    # multi-call, same scope
```

Both work.  The break is when the closure leaves its binding
scope — `call_with(c.get)` or `stash = c.get` (in a function).

## What we know

The FA must track each closure's bound `self` slot through
whatever variable holds it.  When the closure stays in one
function's scope, the FA's per-EntrySet AVar bookkeeping has
the data it needs.  When the closure crosses a function-call
edge, the receiving function's AVar for the closure parameter
has no `bound_self` AType — and downstream uses of the closure
walk a null edge.

The pattern is exactly the v1-era F.4.8 shortcut's blind spot:
"works when the FA pre-resolves the call site, breaks the
moment the closure becomes an opaque value."  The v2 LLVM
closure handler (commit `db4270a`) fixed the call-site-
dispatch half via real `CG2_FIELD_LOAD` from the closure
struct.  This issue is the construction-and-tracking half:
the frontend must build a real closure struct at the period
SEND **and** propagate the type across function boundaries.

Code pointers:
- `python_ifa_build_if1.cc:585` — `lower_send_period`'s closure
  branch (creates closure struct via `if1_send(sym_clone, ...)`
  when `n->creates` is set).
- `ifa/codegen/codegen_common.cc:92` — `is_closure_var`.
- `ifa/analysis/fa.cc` (somewhere in the call-edge processing) —
  null deref site; needs a debugger session to nail down.

## Proposed fix (superseded — see "2026-07 re-investigation" below)

Investigation steps in order:

1. Catch the segfault under gdb on a checked-out build:
   `gdb --args ./pyc -D. /tmp/case_a.py` then `r`, `bt`.
2. Identify the AVar / EntrySet / call edge that's null.
3. The likely fix is at the construction site: ensure the IF1
   shape for `c.get` is a closure-typed Sym with the right
   `has[]` slots set, **before** the function-call site
   evaluates the actual argument type — so the call edge gets
   a non-null `bound_self` AType.
4. Add an early `fail()` at the segfault site with a useful
   diagnostic so any future regression lands as a compile
   error, not a crash.

## 2026-07 re-investigation

**gdb doesn't work in this sandbox** — `gdb --args ./pyc ... `,
`run`, `bt` hangs indefinitely (confirmed with and without a shell,
with `timeout`, with `set startup-with-shell off`; ptrace appears to
be blocked entirely, not just yama-restricted). Worked around this
by rebuilding pyc with a `clang++ -fsanitize=address
-fno-omit-frame-pointer` compiler wrapper passed as `make
CXX=<wrapper>` (overriding `CXX` rather than `CFLAGS`/`CPPFLAGS`,
since those are appended to later in the Makefile and a command-line
override of `CFLAGS` clobbers the later `+=` include-path appends).
ASan's SIGSEGV handler doesn't need ptrace, so this got a full
symbolized backtrace immediately. Worth remembering for any future
segfault investigation in this environment.

**Both repros still crash on current `main`, exactly as documented.**
Tracing them found this is **not** the FA/type-propagation gap this
issue originally hypothesized (`lower_send_period`/`analysis/fa.cc`)
— the closure's type *does* propagate correctly across a function
boundary. The crashes are elsewhere entirely:

### Case A — two unrelated bounds bugs in `simple_inlining`, not FA at all

Both crashes were in `ifa/optimize/inline.cc`'s `simple_inlining`
pass, which runs **unconditionally** after every FA convergence
(`ifa_optimize()` → `simple_inlining()`, `ifa/ifa.cc:78` — not gated
by the `--fa_inline` flag, which controls a *different* inlining
invocation between FA passes, default off).

1. **`simple_closure_call`** (`inline.cc:120`, now fixed): `PNode *p
   = n->cfg_pred[0];` with no bounds check. A closure passed in as a
   function *parameter* (rather than created earlier in the same
   function's body) can be the very first PNode of the function —
   `cfg_pred` is legitimately empty at function entry. Confirmed by
   adding an unrelated statement before `print(f())` in Case A: the
   crash disappeared entirely and the program printed `42` correctly.
   Fix: bail out (not eligible for this inlining) when `cfg_pred` is
   empty, both at the initial check and inside the backward MOVE-chain
   walk.
2. Fixing (1) surfaced a **second, independent** bounds bug reachable
   from Case B: **`inline_single_pnode`** (`inline.cc:233`):
   `p->rvals[i]` where `i` is a formal-position index computed from
   the *callee*'s declared `has[]` list (`fn->sym->has.index(fs)`),
   assumed to align 1:1 with the *call site*'s own `rvals`. That
   assumption doesn't hold for an escaped-closure call site, whose
   `rvals` layout differs from a plain direct call — `i` ran past
   `p->rvals`'s bounds. Fix: bail out (leave the call as a real,
   uninlined dispatch) when `i >= p->rvals.n`, mirroring the existing
   `Type_SUM` bail-out a few lines below (same function, same idiom).

**Case A is now fully fixed** on both backends (confirmed: compiles
and prints `42`). Fixing it also revealed that **a closure returned
from a function** (not just passed as a parameter) already works
correctly end to end — a new, previously-unverified escape shape,
confirmed via a fresh test and added as
`tests/closure_returned_from_function.py`.

### Case B — narrowed to a real, different, still-open codegen bug: closures don't survive a *global* variable

Fixing both `simple_inlining` bugs did **not** fix Case B — it now
gets past the optimizer (no more crash there) and fails later, in
codegen:

```
#0 c_rhs(Var*)                              cg.cc:189
#1 write_send_arg(...)                      cg.cc:661
#2 CBackendEmitter::emit_send_call(PNode*)  cg.cc:765
```

The LLVM backend's verifier gives a much clearer signal for the
same underlying problem: `Incorrect number of arguments passed to
called function! %0 = call i64 @_CG_f_4069_1()` — the call is
emitted with **zero arguments**, even though the callee needs at
least the closure's bound receiver. `write_send_arg`
(`cg.cc:633-645`) has an `is_closure_var(v0)` branch specifically to
unpack a closure struct's fields into the right call-site argument
positions; for this call site, `v0` (the closure value read back
from the global) apparently isn't recognized as `is_closure_var`
(or its `type->has` is empty), so codegen falls through to treating
it as an ordinary, argument-less call.

**A targeted follow-up isolated this precisely to globals**,
narrower than "any closure crossing a function boundary" (which this
issue's title and original framing assumed): a closure crossing a
function boundary via a **parameter** (Case A) or a **return value**
(new test, confirmed working) is fine on both backends. Only a
closure **stored into and read back from a `global`-declared
variable** still breaks. Plausible cause (not yet confirmed by
tracing further): `GLOBAL_CONTOUR` AVars are shared process-wide,
not specialized per call site the way parameter/return-value flows
are — the global's inferred closure type at the read site may not
carry the same per-instance `has[]` field-shape refinement that
parameter passing and return values get from FA's per-EntrySet
specialization.

Not fixed — this is a distinct, real bug from the two
`simple_inlining` crashes, needs its own investigation into how (or
whether) FA refines a `global`-declared Sym's closure type at each
read site. Filing as the open remainder of this issue rather than
guessing further without tracing it directly.

**Follow-up: confirmed precisely, via temporary debug tracing in
`emit_send_call`** (added, traced, then removed — not committed).
Comparing the two call sites directly:

```
Case A (works):   rval[0]: sym=f     type=closure
Case B (crashes): rval[0]: sym=stash type=?
```

`f` (a function parameter, Case A) correctly carries a resolved
`Type_FUN` closure type (`is_closure_var` recognizes it, and
`write_send_arg`'s closure-unpacking branch fires correctly). `stash`
(read back from a global, Case B) has **no resolved type at all** at
the call site — `v0->type` is null. Since `is_closure_var` requires a
non-null `Type_FUN` type, the closure-unpacking branch never fires;
and separately, the codegen loop over `target->positional_arg_positions`
(which reported `n=2` — the callee genuinely expects arguments) skips
every position because `!av->live` for both — FA marked the formal
argument Vars for this call target as **dead**, so nothing gets
written, producing the observed zero-argument call.

This confirms the earlier hypothesis: closures don't get correctly
typed once round-tripped through a `global`-declared Sym. Almost
certainly because `GLOBAL_CONTOUR` AVars (used for module-level
globals) don't participate in FA's normal per-EntrySet/per-call-site
specialization — the mechanism that gives locals, parameters, and
return values their precise per-instance closure `has[]` shape.
`stash`'s single shared global AVar apparently never gets refined to
a concrete closure shape at all.

Properly fixing this means extending FA's global-Sym handling to
correctly propagate and refine closure shapes through
`GLOBAL_CONTOUR` — genuine, non-trivial FA work, comparable in scope
to [007-decorators-not-applied.md](007-decorators-not-applied.md)'s
polymorphic-dispatch investigation, not a quick patch. Deferring
further work on this specific remainder pending direction on whether
it's worth the investment now — the two `simple_inlining` crashes
(the actual majority of this issue's original repro surface) are
fixed and verified; storing a callable in a global and calling it
from elsewhere remains a known, narrow limitation.

## Verification plan

1. ~~Reproduce: both Case A and Case B above segfault on current
   `main`.~~ Done — confirmed both still crashed before this fix,
   via the same two ASan-instrumented backtraces above.
2. ~~Land the fix; rerun.~~ Done for Case A (see above); Case B still
   fails, now via a different, narrower, documented bug (see above).
3. ~~Re-add `tests/escaped_closure.py`~~ Done, with `.exec.check`
   containing `42\n` — this is Case A, now passing.
4. ~~Both backends must pass~~ — `./test_pyc -k escaped_closure` and
   `PYC_FLAGS=-b ./test_pyc -k escaped_closure` both pass. (The v2
   LLVM env var mentioned in the original plan,
   `IFA_LLVM_V2=1`, no longer appears to be a live flag in the
   current build — plain `PYC_FLAGS=-b` already selects the LLVM
   backend and was sufficient.)
5. Full regression suite: `./test_pyc` 124 passed / 0 failed;
   `PYC_FLAGS=-b ./test_pyc` 124 passed / 0 failed (one run during
   this investigation showed a transient `expr_evaluator.py` COMPILE
   failure that did not reproduce in 3 immediate reruns, or in
   isolation via `-k expr_evaluator` — consistent with issue 021's
   documented pointer-hash iteration-order nondeterminism, not a
   regression from this fix). `ifa/ifa-test`: 14 passed, 0 failed.
6. Added `tests/closure_returned_from_function.py` (`.exec.check`
   `42\n`) as a second, independently-confirmed escape shape not in
   the original verification plan.
7. ~~Case B's remaining codegen bug (global-stored closures) is not
   yet fixed~~ Fixed — see "2026-07-04 Case B root cause and fix"
   below. `tests/closure_in_global.py` added; full suite 124/124 on
   both backends, `ifa/ifa-test` 14/14.

## 2026-07-04 Case B root cause and fix

Re-traced from scratch (temporary env-gated tracing in
`write_send_arg`, `-v -v -v` `.dead_log` inspection). Two of the
earlier hypotheses turned out stale or wrong; the real chain:

- **FA propagates the closure through the global just fine.** The
  GLOBAL_CONTOUR AVar for `stash` had
  `out = ( __pyc_None_type__ closure [ get, c:Counter ] )`; dispatch
  at `stash()` resolved to the lambda and typed the result `int64`.
  The earlier "`stash` has no resolved type at all / formals dead"
  trace no longer reproduces (both formals were live and
  `v->type` non-null) — evidently fixed in the interim by the
  fibheap-era codegen/liveness work.
- **The killer is specifically the `stash = None` initializer.**
  Globals are exempt from SSU renaming (`ssu.cc`'s
  `new_Var`/`get_Var` return the original Var for `!is_local`), so
  a global has ONE flow-insensitive Var/AVar program-wide, and the
  dead `None` store unions into every read. Concretization made
  `stash`'s type `Type_SUM{__pyc_None_type__, closure}`. Control:
  the identical program **without** the `None` initializer already
  worked, as did the local-variable equivalent (`f = None;
  f = c.get; f()` — SSU splits the local, the read sees only the
  closure).
- **Codegen couldn't call through the SUM.** `is_closure_var`
  required a bare `Type_FUN`, so `write_send_arg` fell into the
  direct-call path, indexed `n->rvals[1]` on a 1-element `rvals`,
  and `c_rhs` dereferenced the out-of-bounds garbage — the
  segfault. (The LLVM v2 path bounds-checks, which is why it
  produced the zero-argument-call verifier error instead.)

The fix, in keeping with the existing nullable-pointer idiom
(`assign_type_cg_strings_pass2` already gives
`SUM{nil_type, T}` the C type of `T`, since pyc's
`__pyc_None_type__` IS `sym_nil_type`):

- `ifa/codegen/codegen_common.{h,cc}`: new `closure_fun_type(Var*)`
  — returns the effective closure `Type_FUN` Sym, looking through a
  two-member `SUM{nil_type, closure}`; `is_closure_var` reimplemented
  on top of it.
- `ifa/codegen/cg.cc` `write_send_arg`: uses `closure_fun_type`
  for the unpack; plus a bounds check that turns any future
  rvals-index overrun into a `codegen_fail` with a source location
  instead of a segfault.
- `ifa/codegen/cg_emit_llvm.cc`: same `closure_fun_type` switch in
  the closure-unpack branch of call emission.

Verified working: Case B exact repro (42, both backends), the
None-guarded variant (`if stash is not None: stash()`), and the
no-initializer variant. Known boundary: **two different closure
shapes** stored in one global (`stash = a.get` / `stash = b.geta`
on different classes) compiles but fails at runtime with the
defined `matching function not found` assertion — that is the
polymorphic-dispatch gap tracked in `ifa/issues/029`/`030`, not
this issue.

## What this unblocks

- Higher-order functions (`map`, `filter`, callback APIs) where the
  closure is passed as a parameter or returned — now works.
- Event-handler / observer patterns that stash a callback in a
  **global** — now works for a single closure shape
  (`closure_in_global.py`); multiple shapes through one global
  await `ifa/issues/029`/`030` polymorphic dispatch.
- A second and third closure test in the suite that don't trivially
  reduce to the FA pre-resolving everything statically
  (`escaped_closure.py`, `closure_returned_from_function.py`,
  now also `closure_in_global.py`).
