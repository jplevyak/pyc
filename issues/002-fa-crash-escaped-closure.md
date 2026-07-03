# Issue 002: Segfault when a bound-method closure escapes its binding scope

**Status:** open.
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

## Proposed fix

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

## Verification plan

1. Reproduce: both Case A and Case B above segfault on current
   `main` (`b24bfbb`).
2. Land the fix; rerun.
3. Re-add `tests/escaped_closure.py` (dropped in `b24bfbb`):

   ```python
   class Counter:
     v = 0
     get = lambda y: y.v

   def call_with(f):
     print(f())

   c = Counter(); c.v = 42
   call_with(c.get)
   ```

   with `.exec.check` containing `42\n`.
4. Both backends must pass:
   - `./test_pyc -k escaped_closure`
   - `IFA_LLVM_V2=1 PYC_FLAGS=-b ./test_pyc -k escaped_closure`
5. The v2 LLVM closure dispatcher already handles this if the
   frontend produces the right shape — `lower_send_call` reads
   any closure field via `CG2_FIELD_LOAD`.

## What this unblocks

- Higher-order functions (`map`, `filter`, callback APIs).
- Event-handler / observer patterns.
- Any realistic stdlib porting that needs closures to escape
  their binding scope.
- A second closure test in the suite that doesn't trivially
  reduce to the FA pre-resolving everything statically.
